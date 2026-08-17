#include <stdio.h>
#include "hps.h"

class MyErrorHandler: public HPS::EventHandler {
  public:
    MyErrorHandler(): HPS::EventHandler() {}

    virtual ~MyErrorHandler() { Shutdown(); }

    // Override to provide behavior for an error event
    virtual HandleResult Handle(HPS::Event const* in_event)
    {
        HPS::ErrorEvent const* error = static_cast<HPS::ErrorEvent const*>(in_event);
        fprintf(stderr, "Error: %s \n", error->message.GetBytes());
        return HandleResult::Handled;
    }
};

class MyWarningHandler: public HPS::EventHandler {
  public:
    MyWarningHandler(): HPS::EventHandler() {}

    virtual ~MyWarningHandler() { Shutdown(); }

    // Override to provide behavior for a warning event
    virtual HandleResult Handle(HPS::Event const* in_event)
    {
        HPS::WarningEvent const* warning = static_cast<HPS::WarningEvent const*>(in_event);
        fprintf(stderr, "Warning: %s \n", warning->message.GetBytes());
        return HandleResult::Handled;
    }
};