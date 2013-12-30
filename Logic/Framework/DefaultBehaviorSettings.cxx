#include "DefaultBehaviorSettings.h"
#include "ColorMap.h"
#include "SNAPRegistryIO.h"

DefaultBehaviorSettings::DefaultBehaviorSettings()
{
  // Behaviors
  m_LinkedZoomModel = NewSimpleProperty("LinkedZoom", true);
  m_ContinuousMeshUpdateModel = NewSimpleProperty("ContinuousMeshUpdate", false);
  m_SynchronizationModel = NewSimpleProperty("Synchronization", true);
  m_SyncCursorModel = NewSimpleProperty("SyncCursor", true);
  m_SyncZoomModel = NewSimpleProperty("SyncZoom", true);
  m_SyncPanModel = NewSimpleProperty("SyncPan", true);

  // Permissions
  m_CheckForUpdatesModel = NewSimpleProperty("CheckForUpdates", true);

  // Overlay behavior
  m_OverlayColorMapPresetModel =
      NewSimpleProperty("OverlayColorMapPreset",
                        std::string(ColorMap::GetPresetName(ColorMap::COLORMAP_GREY)));

  // Layer layout (uses enum)
  m_OverlayLayoutModel = NewSimpleEnumProperty("OverlayLayout", LAYOUT_STACKED,
                                               SNAPRegistryIO::GetEnumMapLayerLayout());
}
