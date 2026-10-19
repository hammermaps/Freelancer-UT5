"""Erzeugt /Game/Freelancer/Maps/L_ShipView: ein importiertes Schiff mit Licht und Kamera (zum Pruefen von Import und Ausrichtung).

  UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=Tools/create_ship_view_level.py -unattended -nullrhi
"""
import unreal

SHIP_MODEL = '/Game/Imported/Models/DA_Model_li_fighter.DA_Model_li_fighter'
MAP = '/Game/Freelancer/Maps/L_ShipView'

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    unreal.EditorAssetLibrary.delete_asset(MAP)  # abgeleitetes Level, wird neu erzeugt
# Wichtig: das alte EditorLevelLibrary.new_level wechselt das Level nicht zuverlaessig -> Actors landeten im Startlevel
if not level_subsystem.new_level(MAP):
    raise RuntimeError('Level konnte nicht angelegt werden: ' + MAP)
current = unreal.EditorLevelLibrary.get_editor_world().get_path_name()
if 'L_ShipView' not in current:
    raise RuntimeError('Falsches aktives Level: ' + current)
actors = unreal.EditorLevelLibrary

ship_cls = unreal.load_class(None, '/Script/Freelancer.FLShipVisual')
ship = actors.spawn_actor_from_class(ship_cls, unreal.Vector(0, 0, 0))
model = unreal.load_asset(SHIP_MODEL)
if model is None:
    raise RuntimeError('Modell nicht importiert: ' + SHIP_MODEL)
ship.set_editor_property('model', model)
ship.call_method('RebuildFromModel')

def movable_light(cls, location, rotation, intensity):
    """Statische Lichter waeren ohne Lighting-Build schwarz: beweglich anlegen."""
    actor = actors.spawn_actor_from_class(cls, location, rotation)
    comp = actor.get_component_by_class(unreal.DirectionalLightComponent if cls == unreal.DirectionalLight else unreal.SkyLightComponent)
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('intensity', intensity)
    return actor


# Rotator im Python-API: (roll, pitch, yaw)
movable_light(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -40, 150), 8.0)    # Hauptlicht
movable_light(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -20, -30), 3.0)    # Gegenlicht
movable_light(unreal.SkyLight, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0), 1.0)

# Startpunkt des Spielers schraeg von vorn-rechts-oben mit Blick auf das Schiff (Nase zeigt nach +X);
# der Standard-Pawn des GameMode startet dort, so stimmt die Ansicht ohne Kamera-Actor
start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(1400, 900, 380), unreal.Rotator(0, -14, 213))

level_subsystem.save_current_level()
print('L_ShipView gespeichert')
