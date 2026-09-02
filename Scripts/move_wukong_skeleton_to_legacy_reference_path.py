"""
Fix the actual missing Skeleton package path recorded inside the imported
Wukong mesh and animation assets.

The affected assets reference:
    /Game/BlackMythWukong/人物/悟空人物/悟空人/
        悟空_SK_Wukong_LOD0_001_Skeleton

Earlier recovery created a valid Skeleton at:
    /Game/人物/悟空人物/悟空人/
        悟空_SK_Wukong_LOD0_001_Skeleton

Those are different package paths. This script moves the valid Skeleton to the
exact legacy path the imported Wukong assets are asking for. It does NOT load
the broken Animation Sequences, so it is safe while they still report
"Invalid USkeleton supplied".

Run in Unreal Editor:
    Tools -> Execute Python Script...

Preparation:
    * Close all Wukong Mesh / Skeleton / Animation / Montage editor tabs.
    * Back up Content/人物/悟空人物/悟空人 first.
    * Run once with DRY_RUN = True. Then set it to False and run again.
    * Restart Unreal Editor immediately after an APPLY pass.
"""

import unreal


# The valid Skeleton created during the previous repair.
SOURCE_SKELETON = (
    "/Game/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton"
)

# This exact package path appears in CoreCombat.log as the missing dependency.
LEGACY_SKELETON = (
    "/Game/BlackMythWukong/人物/悟空人物/悟空人/"
    "悟空_SK_Wukong_LOD0_001_Skeleton"
)

DRY_RUN = True


def main():
    library = unreal.EditorAssetLibrary

    unreal.log("=== Wukong legacy Skeleton path repair ===")
    unreal.log("Source (valid): {}".format(SOURCE_SKELETON))
    unreal.log("Required legacy path: {}".format(LEGACY_SKELETON))
    unreal.log("Dry run: {}".format(DRY_RUN))

    if not library.does_asset_exist(SOURCE_SKELETON):
        raise RuntimeError(
            "Valid source Skeleton does not exist:\n{}".format(SOURCE_SKELETON)
        )

    source = library.load_asset(SOURCE_SKELETON)
    if not source or not isinstance(source, unreal.Skeleton):
        raise RuntimeError(
            "Source asset is not a valid Skeleton:\n{}".format(SOURCE_SKELETON)
        )

    if library.does_asset_exist(LEGACY_SKELETON):
        raise RuntimeError(
            "The target legacy path is already occupied:\n{}\n"
            "Stop here and inspect it in Content Browser; do not overwrite it."
            .format(LEGACY_SKELETON)
        )

    if DRY_RUN:
        unreal.log(
            "[DRY RUN] Would move valid Skeleton:\n  {}\n  -> {}".format(
                SOURCE_SKELETON, LEGACY_SKELETON
            )
        )
        unreal.log_warning(
            "No asset was changed. Set DRY_RUN = False after confirming this "
            "matches the missing package path printed in Output Log."
        )
        return

    if not library.rename_asset(SOURCE_SKELETON, LEGACY_SKELETON):
        raise RuntimeError(
            "Rename failed. No changes should have been applied to the source "
            "Skeleton."
        )

    if not library.save_asset(LEGACY_SKELETON, only_if_is_dirty=False):
        raise RuntimeError(
            "The Skeleton was moved but could not be saved. Save it manually "
            "from Content Browser before closing the editor."
        )

    unreal.log("=== SUCCESS ===")
    unreal.log(
        "The valid Skeleton now exists at the exact package path referenced "
        "by the imported Wukong Mesh and animation assets."
    )
    unreal.log_warning(
        "Restart Unreal Editor now. Then open AS_Wukong_stand01 first."
    )


if __name__ == "__main__":
    main()
