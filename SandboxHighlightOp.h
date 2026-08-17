#pragma once

#include "sprk_ops.h"

class HPSWidget;

class SandboxHighlightOperator: public HPS::SelectOperator {
  public:
    SandboxHighlightOperator(HPSWidget* widget);
    virtual ~SandboxHighlightOperator();

    virtual HPS::UTF8 GetName() const { return "Qt_SandboxHighlightOperator"; }

    virtual bool OnMouseDown(HPS::MouseState const& in_state);
    virtual bool OnMouseUp(HPS::MouseState const& in_state);
    virtual bool OnTouchDown(HPS::TouchState const& in_state);

    static ::HPS::Selection::Level SelectionLevel;

  private:
    void HighlightCommon();

    HPSWidget* mainWidget;

    // Empty-space handling for the shape map: an empty-space mouse-down starts an orbit, so we defer
    // clearing the map highlight to mouse-up and only clear on a click (no drag), not on an orbit.
    bool m_clearHighlightOnUp = false;
    HPS::WindowPoint m_downLocation;
};
