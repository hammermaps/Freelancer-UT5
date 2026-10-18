# Creates the empty test level /Game/Freelancer/Maps/L_Test_Space (run via UnrealEditor-Cmd -ExecutePythonScript)
import unreal

path = "/Game/Freelancer/Maps"
name = "L_Test_Space"
if not unreal.EditorAssetLibrary.does_directory_exist(path):
    unreal.EditorAssetLibrary.make_directory(path)
full = f"{path}/{name}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.log("Level already exists: " + full)
else:
    ok = unreal.EditorLevelLibrary.new_level(full)
    unreal.log(f"new_level({full}) -> {ok}")
    if ok:
        unreal.EditorLevelLibrary.save_current_level()
