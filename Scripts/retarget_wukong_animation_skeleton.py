"""
Reassign every animation asset under the Wukong action folder to the repaired
Skeleton asset.

Run this script from Unreal Editor:
    Tools -> Execute Python Script...

It intentionally changes only animation assets (Animation Sequence, Montage,
Blend Space, Aim Offset, Pose Asset, Composite). It skips Skeletons, Skeletal
Meshes, Materials, Blueprints, and every asset outside ACTION_ROOT.

Before running:
    1. Make a copy of Content/人物/悟空人物/悟空人.
    2. Ensure the repaired Skeleton below is the Skeleton used by the Wukong
       Skeletal Mesh.
    3. Close any Animation Sequence / Montage editors for Wukong assets.
"""

import unreal


# ---------- Configuration ----------

ACTION_ROOT = "/Game/人物/悟空人物/悟空人/动作"
TARGET_SKELETON = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton1.悟空_SK_Wukong_LOD0_001_Skeleton1"
)

# Set to True for a safe report-only pass. No assets will be changed or saved.
DRY_RUN = True

# Save all successfully changed packages when DRY_RUN is False.
SAVE_CHANGED_ASSETS = True

# Never change any asset whose target Skeleton is already correct.
SKIP_ALREADY_ASSIGNED = True


def asset_path(asset):
    return str(asset.get_path_name())


def get_skeleton_path(asset):
    """Return the current Skeleton object path, or '<None>' when missing."""
    try:
        skeleton = asset.get_editor_property("skeleton")
    except Exception:
        return "<No skeleton property>"

    return asset_path(skeleton) if skeleton else "<None>"


def is_supported_animation_asset(asset):
    # Covers Animation Sequence, Montage, Blend Space, Aim Offset, Pose Asset,
    # Composite, and other Unreal animation-asset subclasses without touching
    # Skeletal Meshes, Skeletons, Blueprints, Materials, etc.
    return isinstance(asset, unreal.AnimationAsset)


def main():
    asset_library = unreal.EditorAssetLibrary
    target_skeleton = asset_library.load_asset(TARGET_SKELETON)

    if not target_skeleton:
        raise RuntimeError(
            "Cannot load TARGET_SKELETON:\n{}".format(TARGET_SKELETON)
        )

    if not isinstance(target_skeleton, unreal.Skeleton):
        raise RuntimeError(
            "TARGET_SKELETON is not a Skeleton asset:\n{}".format(
                TARGET_SKELETON
            )
        )

    all_paths = asset_library.list_assets(
        ACTION_ROOT,
        recursive=True,
        include_folder=False,
    )

    changed_assets = []
    skipped_assets = []
    failed_assets = []

    unreal.log("=== Wukong animation Skeleton reassignment ===")
    unreal.log("Action root: {}".format(ACTION_ROOT))
    unreal.log("Target Skeleton: {}".format(TARGET_SKELETON))
    unreal.log("Dry run: {}".format(DRY_RUN))
    unreal.log("Assets found: {}".format(len(all_paths)))

    with unreal.ScopedSlowTask(
        len(all_paths), "Reassigning Wukong animation Skeleton"
    ) as slow_task:
        slow_task.make_dialog(True)

        for path in all_paths:
            slow_task.enter_progress_frame(1, path)

            if slow_task.should_cancel():
                unreal.log_warning("Cancelled by user.")
                break

            asset = asset_library.load_asset(path)
            if not asset:
                failed_assets.append((path, "Could not load asset"))
                unreal.log_error("[FAILED] {} (could not load)".format(path))
                continue

            if not is_supported_animation_asset(asset):
                skipped_assets.append((path, asset.get_class().get_name()))
                continue

            old_skeleton_path = get_skeleton_path(asset)
            if (
                SKIP_ALREADY_ASSIGNED
                and old_skeleton_path == asset_path(target_skeleton)
            ):
                skipped_assets.append((path, "Already assigned"))
                continue

            if DRY_RUN:
                changed_assets.append(path)
                unreal.log(
                    "[DRY RUN] {} | {} -> {}".format(
                        path,
                        old_skeleton_path,
                        asset_path(target_skeleton),
                    )
                )
                continue

            try:
                asset.modify()
                asset.set_editor_property("skeleton", target_skeleton)
                changed_assets.append(path)
                unreal.log(
                    "[CHANGED] {} | {} -> {}".format(
                        path,
                        old_skeleton_path,
                        asset_path(target_skeleton),
                    )
                )
            except Exception as error:
                failed_assets.append((path, str(error)))
                unreal.log_error("[FAILED] {} | {}".format(path, error))

    if not DRY_RUN and SAVE_CHANGED_ASSETS and changed_assets:
        unreal.log("Saving {} changed asset(s)...".format(len(changed_assets)))
        for path in changed_assets:
            if not asset_library.save_asset(path, only_if_is_dirty=True):
                failed_assets.append((path, "Save failed"))
                unreal.log_error("[FAILED TO SAVE] {}".format(path))

    unreal.log("=== Reassignment complete ===")
    unreal.log("Changed / would change: {}".format(len(changed_assets)))
    unreal.log("Skipped: {}".format(len(skipped_assets)))
    unreal.log("Failed: {}".format(len(failed_assets)))

    if failed_assets:
        unreal.log_warning("Failure details:")
        for path, reason in failed_assets:
            unreal.log_warning("  {} | {}".format(path, reason))

    if DRY_RUN:
        unreal.log_warning(
            "This was a DRY RUN. Inspect Output Log. "
            "Then set DRY_RUN = False and run again to apply changes."
        )


if __name__ == "__main__":
    main()
