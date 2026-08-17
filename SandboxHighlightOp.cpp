#include "SandboxHighlightOp.h"
#include "HPSWidget.h"

HPS::Selection::Level SandboxHighlightOperator::SelectionLevel = HPS::Selection::Level::Entity;

SandboxHighlightOperator::SandboxHighlightOperator(HPSWidget* widget):
    HPS::SelectOperator(HPS::MouseButtons::ButtonLeft(), HPS::ModifierKeys()), mainWidget(widget)
{
}

SandboxHighlightOperator::~SandboxHighlightOperator() {}

bool SandboxHighlightOperator::OnMouseDown(HPS::MouseState const& in_state)
{
    if (!IsMouseTriggered(in_state))
        return false;

    auto sel_opts = GetSelectionOptions();
    sel_opts.SetLevel(SandboxHighlightOperator::SelectionLevel);
    SetSelectionOptions(sel_opts);

    // Perform the pick. We deliberately ignore SelectOperator::OnMouseDown's return value: on an
    // empty-space click it still returns true (it "handled" the event by clearing the selection),
    // which would swallow the gesture and prevent the Orbit operator beneath us from rotating.
    // Instead we decide hit/miss from the actual selection count.
    HPS::SelectOperator::OnMouseDown(in_state);
    if (GetActiveSelection().GetCount() > 0) {
        m_clearHighlightOnUp = false; // a hit: highlight it, nothing to clear later
        HighlightCommon();
        return true; // a marker/part was hit -> consume the event
    }
    // Empty-space mouse-down: this also starts an orbit, so don't clear the map highlight yet.
    // Defer the decision to mouse-up and only clear if the user clicked without dragging (see
    // OnMouseUp). Let the event fall through to the Orbit operator below so the view can rotate.
    m_clearHighlightOnUp = true;
    m_downLocation = in_state.GetLocation();
    return false;    // miss -> fall through to the Orbit operator below and rotate
}

bool SandboxHighlightOperator::OnMouseUp(HPS::MouseState const& in_state)
{
    if (m_clearHighlightOnUp) {
        m_clearHighlightOnUp = false;
        // Only treat this as a click (clear the highlight) if the pointer barely moved; a drag is an
        // orbit and must keep the current highlight.
        HPS::WindowPoint const up = in_state.GetLocation();
        float const dx = up.x - m_downLocation.x;
        float const dy = up.y - m_downLocation.y;
        if ((dx * dx + dy * dy) < (0.01f * 0.01f)) {
            if (mainWidget->ClearShapeMapHighlight())
                mainWidget->getCanvas()->Update();
        }
    }
    return false;    // never consume the up: let the Orbit operator finish its gesture
}

bool SandboxHighlightOperator::OnTouchDown(HPS::TouchState const& in_state)
{
    auto sel_opts = GetSelectionOptions();
    sel_opts.SetLevel(SandboxHighlightOperator::SelectionLevel);
    SetSelectionOptions(sel_opts);

    HPS::SelectOperator::OnTouchDown(in_state);
    if (GetActiveSelection().GetCount() > 0) {
        HighlightCommon();
        return true;
    }
    return false;
}

void SandboxHighlightOperator::HighlightCommon()
{
    HPS::SelectionResults selection_results = GetActiveSelection();

    // When a shape map is on screen the scene holds only the marker cloud (no CADModel), so treat a
    // pick as a shape-map selection: highlight the point and select the matching part in the panel.
    if (mainWidget->PickShapeMapPart(selection_results)) {
        mainWidget->getCanvas()->Update();
        return;
    }

    mainWidget->Unhighlight();

    size_t selected_count = selection_results.GetCount();
    if (selected_count > 0) {
        HPS::CADModel cad_model = mainWidget->getCADModel();

        HPS::HighlightOptionsKit highlight_options(HPS::HighlightOptionsKit::GetDefault());
        highlight_options.SetStyleName("highlight_style");
        highlight_options.SetSubentityHighlighting(SelectionLevel == HPS::Selection::Level::Subentity);
        highlight_options.SetOverlay(HPS::Drawing::Overlay::InPlace);

        if (!cad_model.Empty()) {
            // since we have a CADModel, we want to highlight the components, not just the Visualize geometry
            HPS::SelectionResultsIterator it = selection_results.GetIterator();
            HPS::Canvas canvas = *mainWidget->getCanvas();
            while (it.IsValid()) {
                HPS::ComponentPath component_path = cad_model.GetComponentPath(it.GetItem());
                if (!component_path.Empty()) {
                    // Make the selected component get highlighted in the model browser
                    highlight_options.SetNotification(true);
                    component_path.Highlight(canvas, highlight_options);

                    // if we selected PMI, highlight the associated components (if any)
                    HPS::Component const& leaf_component = component_path.Front();
                    if (leaf_component.HasComponentType(HPS::Component::ComponentType::ExchangePMIMask)) {
                        // Only highlight the Visualize geometry for the associated components, don't highlight the associated
                        // components in the model browser
                        highlight_options.SetNotification(false);
                        for (auto const& reference: leaf_component.GetReferences())
                            HPS::ComponentPath(1, &reference).Highlight(canvas, highlight_options);
                    }
                }
                it.Next();
            }
        }
        else {
            // since there is no CADModel, just highlight the Visualize geometry
            mainWidget->getCanvas()->GetWindowKey().GetHighlightControl().Highlight(selection_results, highlight_options);
            HPS::Database::GetEventDispatcher().InjectEvent(
                HPS::HighlightEvent(HPS::HighlightEvent::Action::Highlight, selection_results, highlight_options));
        }
    }

    mainWidget->getCanvas()->Update();
}
