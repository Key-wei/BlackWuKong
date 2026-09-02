"""
Install the VALID repaired Wukong Skeleton at the package path that all broken
Wukong animation assets reference.

Current situation
-----------------
* A package already exists at the legacy reference path, but it is the old
  imported Skeleton (not the valid Skeleton recreated from the mesh).
* The valid recreated Skeleton currently lives at /Game/人物/...
* The affected animations and Wukong mesh refer to /Game/BlackMythWukong/...

This script preserves the existing legacy package under a backup name, then
moves the valid recreated Skeleton to the exact legacy path. It does not load
any Animation Sequence or Montage, because those packages currently fail while
their Skeleton dependency is unresolved.

Before APPLY:
    1. Close all Wukong asset editor tabs.
    2. Back up Content/人物/悟空人物/悟空人 and
       Content/BlackMythWukong/人物/悟空人物/悟空人.
    3. Run with DRY_RUN=True and inspect the Output Log.
    4. Set DRY_RUN=False and run once.
    5. Restart Unreal Editor immediately.
"""

import unreal


# The valid Skeleton recreated from the Wukong Skeletal Mesh.
VALID_SOURCE_SKELETON = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton"
)

# The exact path reported as a missing dependency in CoreCombat.log.
LEGACY_TARGET_SKELETON = (
    "/Game/BlackMythWukong/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton"
)

# Existing package at LEGACY_TARGET_SKELETON will be kept at this path.
LEGACY_TARGET_BACKUP = (
    "/Game/BlackMythWukong/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton_PreRepair_20260901"
)

DRY_RUN = False


def require_skeleton(asset_path, label):
    library = unreal.EditorAssetLibrary
    if not library.does_asset_exist(asset_path):
        raise RuntimeError("{} does not exist:\n{}".format(label, asset_path))

    asset = library.load_asset(asset_path)
    if not asset or not isinstance(asset, unreal.Skeleton):
        actual_type = asset.get_class().get_name() if asset else "<unloadable>"
        raise RuntimeError(
            "{} is not a loadable Skeleton ({}):\n{}".format(
                label, actual_type, asset_path
            )
        )
    return asset


def main():
    library = unreal.EditorAssetLibrary

    unreal.log("=== Install valid Wukong Skeleton at legacy path ===")
    unreal.log("Valid source: {}".format(VALID_SOURCE_SKELETON))
    unreal.log("Legacy target: {}".format(LEGACY_TARGET_SKELETON))
    unreal.log("Legacy backup: {}".format(LEGACY_TARGET_BACKUP))
    unreal.log("Dry run: {}".format(DRY_RUN))

    require_skeleton(VALID_SOURCE_SKELETON, "Valid source Skeleton")
    require_skeleton(LEGACY_TARGET_SKELETON, "Existing legacy Skeleton")

    if library.does_asset_exist(LEGACY_TARGET_BACKUP):
        raise RuntimeError(
            "Backup path is already occupied; refusing to overwrite it:\n{}".format(
                LEGACY_TARGET_BACKUP
            )
        )

    if DRY_RUN:
        unreal.log("[DRY RUN] Planned operations:")
        unreal.log(
            "  1) Preserve legacy package:\n     {}\n     -> {}".format(
                LEGACY_TARGET_SKELETON, LEGACY_TARGET_BACKUP
            )
        )
        unreal.log(
            "  2) Delete only the redirector at:\n     {}".format(
                LEGACY_TARGET_SKELETON
            )
        )
        unreal.log(
            "  3) Move valid recreated Skeleton:\n     {}\n     -> {}".format(
                VALID_SOURCE_SKELETON, LEGACY_TARGET_SKELETON
            )
        )
        unreal.log_warning(
            "No assets were changed. Set DRY_RUN = False only after verifying "
            "the backup copies are complete."
        )
        return

    # Preserve the old package first. UE creates an ObjectRedirector at the old
    # location; remove only that redirector to free the target identity.
    if not library.rename_asset(LEGACY_TARGET_SKELETON, LEGACY_TARGET_BACKUP):
        raise RuntimeError(
            "Could not preserve existing legacy Skeleton. No further changes "
            "were attempted."
        )

    if library.does_asset_exist(LEGACY_TARGET_SKELETON):
        if not library.delete_asset(LEGACY_TARGET_SKELETON):
            raise RuntimeError(
                "Could not delete redirector at the legacy target path. "
                "The old package was preserved here:\n{}\n"
                "Stop and restore from backup rather than deleting anything "
                "manually.".format(LEGACY_TARGET_BACKUP)
            )

    if not library.rename_asset(
        VALID_SOURCE_SKELETON, LEGACY_TARGET_SKELETON
    ):
        raise RuntimeError(
            "Could not move the valid Skeleton into the legacy path. "
            "The existing old Skeleton remains preserved at:\n{}".format(
                LEGACY_TARGET_BACKUP
            )
        )

    if not library.save_asset(LEGACY_TARGET_SKELETON, only_if_is_dirty=False):
        raise RuntimeError(
            "Move succeeded but saving failed. Save this asset manually before "
            "closing Unreal Editor:\n{}".format(LEGACY_TARGET_SKELETON)
        )

    unreal.log("=== SUCCESS ===")
    unreal.log(
        "The valid Skeleton now owns the exact package path required by the "
        "Wukong mesh and animation assets."
    )
    unreal.log_warning(
        "Restart Unreal Editor now. Open AS_Wukong_stand01 after restart and "
        "check the Output Log for any remaining load errors."
    )


if __name__ == "__main__":
    main()
