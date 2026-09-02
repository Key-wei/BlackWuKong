"""
Recover Wukong animation assets whose old Skeleton reference is invalid.

Why this script exists
----------------------
Animation assets currently cannot be loaded because their saved Skeleton
reference is invalid. Therefore they cannot be repaired one-by-one with
asset.set_editor_property("skeleton", ...): Unreal must load the asset before
that Python code can execute, and loading fails first.

This script repairs the reference at its source:
    1. Renames the old/broken Skeleton asset to a backup name.
    2. Removes the redirector temporarily occupying the original asset path.
    3. Renames the repaired Skeleton1 asset to the ORIGINAL Skeleton name.

All existing Animation Sequences, Montages, Blend Spaces, etc. already refer
to the original Skeleton object path, so after the identity swap they resolve
to the repaired Skeleton without loading/re-saving each damaged animation.

Run from Unreal Editor:
    Tools -> Execute Python Script...

IMPORTANT
---------
* Close all Wukong animation, Skeleton and Skeletal Mesh editor tabs first.
* Copy Content/人物/悟空人物/悟空人 to a backup location before the APPLY pass.
* Run once with DRY_RUN = True, inspect Output Log, then set it to False.
* Restart the editor immediately after a successful APPLY pass.
"""

import unreal


# ---------- Configuration ----------

# The broken Skeleton path currently embedded in all Wukong animation assets.
OLD_SKELETON_ASSET = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton"
)

# The valid Skeleton that Unreal created from the Wukong Skeletal Mesh.
REPAIRED_SKELETON_ASSET = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton1"
)

# Keep the original asset rather than deleting it.
BROKEN_SKELETON_BACKUP_ASSET = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton_Broken_20260901"
)

# Safe by default. Set to False only after a file-system backup is complete.
DRY_RUN = False


def load_skeleton(asset_path, label):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError("{} could not be loaded: {}".format(label, asset_path))
    if not isinstance(asset, unreal.Skeleton):
        raise RuntimeError(
            "{} is not a Skeleton asset: {} ({})".format(
                label,
                asset_path,
                asset.get_class().get_name(),
            )
        )
    return asset


def require_asset_exists(asset_path, label):
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        raise RuntimeError("{} does not exist: {}".format(label, asset_path))


def main():
    library = unreal.EditorAssetLibrary

    unreal.log("=== Wukong Skeleton identity recovery ===")
    unreal.log("Old Skeleton path: {}".format(OLD_SKELETON_ASSET))
    unreal.log("Repaired Skeleton path: {}".format(REPAIRED_SKELETON_ASSET))
    unreal.log("Broken backup path: {}".format(BROKEN_SKELETON_BACKUP_ASSET))
    unreal.log("Dry run: {}".format(DRY_RUN))

    require_asset_exists(OLD_SKELETON_ASSET, "Old Skeleton")
    require_asset_exists(REPAIRED_SKELETON_ASSET, "Repaired Skeleton")

    if library.does_asset_exist(BROKEN_SKELETON_BACKUP_ASSET):
        raise RuntimeError(
            "Backup asset path already exists. Do not overwrite it:\n{}".format(
                BROKEN_SKELETON_BACKUP_ASSET
            )
        )

    # Validate the repaired Skeleton before touching the original path.
    repaired_skeleton = load_skeleton(
        REPAIRED_SKELETON_ASSET, "Repaired Skeleton"
    )
    unreal.log(
        "Validated repaired Skeleton: {}".format(repaired_skeleton.get_path_name())
    )

    if DRY_RUN:
        unreal.log("[DRY RUN] Would rename:")
        unreal.log(
            "  1) {} -> {}".format(
                OLD_SKELETON_ASSET, BROKEN_SKELETON_BACKUP_ASSET
            )
        )
        unreal.log(
            "  2) Delete redirector left at {}".format(OLD_SKELETON_ASSET)
        )
        unreal.log(
            "  3) {} -> {}".format(
                REPAIRED_SKELETON_ASSET, OLD_SKELETON_ASSET
            )
        )
        unreal.log_warning(
            "No assets were changed. Back up the Wukong folder, set "
            "DRY_RUN = False, and run again."
        )
        return

    # Step 1: preserve the old invalid Skeleton under a distinct package name.
    if not library.rename_asset(
        OLD_SKELETON_ASSET, BROKEN_SKELETON_BACKUP_ASSET
    ):
        raise RuntimeError(
            "Failed to rename old Skeleton to backup. Nothing else was changed."
        )

    # UE leaves an ObjectRedirector at the old path after a rename. It must be
    # removed so the valid Skeleton can take exactly the path that all damaged
    # animation packages still reference.
    if library.does_asset_exist(OLD_SKELETON_ASSET):
        if not library.delete_asset(OLD_SKELETON_ASSET):
            raise RuntimeError(
                "Old path is still occupied and its redirector could not be "
                "deleted. The original Skeleton was preserved here:\n{}\n"
                "Do NOT continue manually; restore the backup folder first."
                .format(BROKEN_SKELETON_BACKUP_ASSET)
            )

    # Step 3: make the valid Skeleton own the original, referenced identity.
    if not library.rename_asset(
        REPAIRED_SKELETON_ASSET, OLD_SKELETON_ASSET
    ):
        raise RuntimeError(
            "Failed to rename repaired Skeleton to original path. "
            "The old Skeleton remains preserved at:\n{}".format(
                BROKEN_SKELETON_BACKUP_ASSET
            )
        )

    if not library.save_asset(OLD_SKELETON_ASSET, only_if_is_dirty=False):
        raise RuntimeError(
            "Identity swap succeeded but saving the repaired Skeleton failed. "
            "Do not close the editor; save it manually from Content Browser."
        )

    unreal.log("=== SUCCESS ===")
    unreal.log(
        "The valid Skeleton now owns the original path referenced by animation assets."
    )
    unreal.log(
        "Restart Unreal Editor now. Then open AS_Wukong_stand01 and verify "
        "that the Skeleton and Preview Mesh resolve correctly."
    )


if __name__ == "__main__":
    main()
