"""Erzeugt /Game/Freelancer/Maps/L_ShipView: ein importiertes Schiff mit Licht und Kamera (zum Pruefen von Import und Ausrichtung).

  UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=Tools/create_ship_view_level.py -unattended -nullrhi
"""
import unreal

SHIP_MODEL = '/Game/Imported/Models/DA_Model_li_fighter.DA_Model_li_fighter'
MAP = '/Game/Freelancer/Maps/L_ShipView'

unreal.EditorLevelLibrary.new_level(MAP)
actors = unreal.EditorLevelLibrary

ship_cls = unreal.load_class(None, '/Script/Freelancer.FLShipVisual')
ship = actors.spawn_actor_from_class(ship_cls, unreal.Vector(0, 0, 0))
ship.set_editor_property('model', unreal.SoftObjectPath(SHIP_MODEL))
ship.call_method('RebuildFromModel')

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -45, -30))
sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', 6.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property('intensity', 1.0)

# Kamera schraeg von vorn-rechts-oben auf das Schiff (Nase zeigt nach +X)
cam = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(2200, 1400, 700), unreal.Rotator(0, -15, 212))
cam.set_editor_property('auto_activate_for_player', unreal.AutoReceiveInput.PLAYER0)

unreal.EditorLoadingAndSavingUtils.save_current_level()
print('L_ShipView gespeichert')
