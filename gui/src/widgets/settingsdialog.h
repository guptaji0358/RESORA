#pragma once
// RESORA Settings (header "SETTINGS" or Ctrl+,). One section today: Saved Locations.
#include "framelessdialog.h"

class LocationStore;
class SavedLocationsPanel;

class SettingsDialog : public FramelessDialog
{
    Q_OBJECT
public:
    SettingsDialog(LocationStore *store, QWidget *parent = nullptr);

    SavedLocationsPanel *locationsPanel() const { return panel_; }

private:
    SavedLocationsPanel *panel_;
};
