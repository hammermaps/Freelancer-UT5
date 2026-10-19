"""Erzeugt /Game/Freelancer/Maps/L_FlightTest: leerer Raum mit mehreren PlayerStarts und Licht fuer Netzwerk-Flugtests.

  UnrealEditor-Cmd Freelancer.uproject -ExecutePythonScript=Tools/create_flight_test_level.py -unattended -nullrhi
Vorher eine vorhandene Datei verschieben (nicht loeschen).
"""
import unreal

MAP = '/Game/Freelancer/Maps/L_FlightTest'
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level_subsystem.new_level(MAP):
    raise RuntimeError('Level konnte nicht angelegt werden: ' + MAP)
if 'L_FlightTest' not in unreal.EditorLevelLibrary.get_editor_world().get_path_name():
    raise RuntimeError('Falsches aktives Level')
actors = unreal.EditorLevelLibrary

# Spielerstarts nebeneinander, Nase zeigt nach +X (Rotator im Python-API: roll, pitch, yaw)
for i, y in enumerate((-2000, 0, 2000, 4000)):
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, y, 0), unreal.Rotator(0, 0, 0))

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(0, -40, 150))
comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
comp.set_editor_property('intensity', 8.0)

level_subsystem.save_current_level()
print('L_FlightTest gespeichert')
