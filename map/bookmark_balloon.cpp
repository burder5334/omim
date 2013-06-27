#include "bookmark_balloon.hpp"
#include "framework.hpp"

#include "../geometry/transformations.hpp"

#include "../anim/controller.hpp"
#include "../anim/value_interpolation.hpp"

namespace
{
  static const int PopupPadding = 23;
  static const int AnimPadding = 8;
}

class BookmarkBalloon::BalloonAnimTask : public anim::Task
{
public:
  BalloonAnimTask(double startScale, double endScale,
                  double startOffset, double endOffset,
                  double interval)
    : m_scaleAnim(startScale, endScale, interval, m_scale),
      m_offsetAnim(startOffset, endOffset, interval, m_offset)
  {
    m_scale = startScale;
    m_offset = startOffset;
  }

  void OnStart(double ts)
  {
    m_scaleAnim.Start();
    m_scaleAnim.OnStart(ts);

    m_offsetAnim.Start();
    m_offsetAnim.OnStart(ts);
    anim::Task::OnStart(ts);
  }

  void OnStep(double ts)
  {
    m_scaleAnim.OnStep(ts);
    m_offsetAnim.OnStep(ts);
    UpdateState();
    anim::Task::OnStep(ts);
  }

  void OnEnd(double ts)
  {
    m_scaleAnim.OnEnd(ts);
    m_offsetAnim.OnEnd(ts);
    UpdateState();
    anim::Task::OnEnd(ts);
  }

  void OnCancel(double ts)
  {
    m_scaleAnim.OnCancel(ts);
    m_offsetAnim.OnCancel(ts);
    UpdateState();
    anim::Task::OnCancel(ts);
  }

  double GetScale() const
  {
    return m_scale;
  }

  double GetOffset() const
  {
    return m_offset;
  }

  bool IsVisual() const
  {
    return true;
  }

private:
  void UpdateState()
  {
    SetState(m_scaleAnim.State());
  }

private:
  anim::ValueInterpolation m_scaleAnim;
  anim::ValueInterpolation m_offsetAnim;
  double m_scale;
  double m_offset;
};

BookmarkBalloon::BookmarkBalloon(Params const & p)
 : Balloon(p),
   m_framework(p.m_framework),
   m_isPositionChecked(true),
   m_needPadding(false)
{
}

void BookmarkBalloon::setIsPositionChecked(bool isChecked)
{
  m_isPositionChecked = isChecked;
}

bool BookmarkBalloon::isPositionChecked() const
{
  return m_isPositionChecked;
}

bool BookmarkBalloon::checkPosition()
{
  if (isPositionChecked())
    return false;

  setIsPositionChecked(true);

  bool result = false;

  bool needLayout = false;
  m2::RectD balloonRect = roughBoundRect();
  if (balloonRect.minX() < 0)
  {
    setPosition(graphics::EPosLeft);
    needLayout = true;
  }
  if (m_framework->GetNavigator().Screen().GetWidth() < balloonRect.maxX())
  {
    setPosition(graphics::EPosRight);
    needLayout = true;
  }

  if (needLayout)
    layout();

  ScreenBase const & screen =  m_framework->GetNavigator().Screen();

  m2::PointD globalOrg = screen.GetOrg();
  m2::PointD pixelOrg = screen.GtoP(globalOrg);

  double k = visualScale();
  balloonRect = roughBoundRect();
  if (balloonRect.minX() < 0)
  {
    pixelOrg.x += (balloonRect.minX() - AnimPadding * k);
    result = true;
  }
  else if (balloonRect.maxX() > screen.GetWidth())
  {
    pixelOrg.x += (balloonRect.maxX() - screen.GetWidth() + AnimPadding * k);
    result = true;
  }

  if (balloonRect.minY() < 0)
  {
    pixelOrg.y += (balloonRect.minY() - AnimPadding * k);
    result = true;
  }
  else if (balloonRect.maxY() > screen.GetHeight())
  {
    pixelOrg.y += (balloonRect.maxY() - screen.GetHeight() + AnimPadding * k);
    result = true;
  }

  m_framework->GetAnimator().MoveScreen(globalOrg, screen.PtoG(pixelOrg), 0.5);

  return result || needLayout;
}

void BookmarkBalloon::update()
{
  Balloon::update();
  if (isVisible())
  {
    m2::PointD newPivot(m_framework->GtoP(m_glbPivot));
    if (m_needPadding)
      newPivot.y -= PopupPadding * visualScale();
    setPivot(newPivot);

    if (checkPosition())
      setIsDirtyLayout(true);

    if (m_currentAnimTask)
    {
      m_balloonScale = m_currentAnimTask->GetScale();
      m_textImageOffsetH = m_currentAnimTask->GetOffset();
    }
  }
}

void BookmarkBalloon::createTask(double startScale, double endScale, double startOffset, double endOffset, double interval, int index)
{
  m_currentAnimTask.reset(new BalloonAnimTask(startScale, endScale, startOffset, endOffset, interval));
  m_currentAnimTask->AddCallback(anim::Task::EEnded, bind(&BookmarkBalloon::animTaskEnded, this, index));
  m_currentAnimTask->AddCallback(anim::Task::ECancelled, bind(&BookmarkBalloon::cancelTask, this));
  m_framework->GetAnimController()->AddTask(m_currentAnimTask);
}

void BookmarkBalloon::animTaskEnded(int animIndex)
{
  bool isVisibleTextAndImage = true;
  switch(animIndex)
  {
  case 0:
    createTask(0.1, 1.05, 0.0, 0.0, 0.1, 1);
    isVisibleTextAndImage = false;
    break;
  case 1:
    createTask(1.05, 0.95, -2.0, 5.0, 0.05, 2);
    break;
  case 2:
    createTask(0.95, 1.0, 5.0, 0.0, 0.02, 3);
    break;
  }

  m_mainTextView->setIsVisible(isVisibleTextAndImage);
  m_auxTextView->setIsVisible(isVisibleTextAndImage);
  m_imageView->setIsVisible(isVisibleTextAndImage);
}

void BookmarkBalloon::cancelTask()
{
  if (m_currentAnimTask)
  {
    m_currentAnimTask->Lock();
    if (!m_currentAnimTask->IsEnded() &&
        !m_currentAnimTask->IsCancelled())
    {
      m_currentAnimTask->Cancel();
    }

    m_currentAnimTask->Unlock();
    m_currentAnimTask.reset();
  }
}

void BookmarkBalloon::showAnimated(bool needPadding)
{
  m_needPadding = needPadding;
  setPosition(graphics::EPosCenter);
  setIsPositionChecked(false);

  animTaskEnded(0);
  setIsVisible(true);
}

void BookmarkBalloon::hide()
{
  cancelTask();
  setIsVisible(false);
}

void BookmarkBalloon::setGlbPivot(m2::PointD const & pivot)
{
  m_glbPivot = pivot;
}

m2::PointD const & BookmarkBalloon::glbPivot() const
{
  return m_glbPivot;
}

void BookmarkBalloon::setBookmarkCaption(string const & name,
                                         string const & type)
{
  m_bmkName = name;
  m_bmkType = type;

  setText(name, type);
}

string BookmarkBalloon::bookmarkName() const
{
  if (!m_bmkName.empty() && !m_bmkType.empty())
    return m_bmkName + " (" + m_bmkType + ")";

  return m_bmkName.empty() ? m_bmkType : m_bmkName;
}
