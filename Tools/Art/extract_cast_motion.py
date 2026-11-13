"""Extract right-arm gesture samples from KayKit Character Animations 1.1 Free."""

from hashlib import sha256
from math import floor
from pathlib import Path
import sys

import bpy


SOURCE_HASH = "b70e1d5ef64fafaad5c50ca2225c238f33ec9622876014f38478deeb10ee2ba4"
CLIPS = ("Shoot", "Spellcasting", "SpellcastingLong", "Raise")
SAMPLE_COUNT = 9


def render_header(source):
    if sha256(source.read_bytes()).hexdigest() != SOURCE_HASH:
        raise ValueError("Expected Rig_Medium_CombatRanged.fbx from KayKit Character Animations 1.1 Free")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    scene = bpy.context.scene
    rig = bpy.data.objects["Rig_Medium"]
    rest = rig.data.bones
    shoulder = rest["upperarm.r"].head_local
    up = (rest["chest"].head_local - rest["hips"].head_local).normalized()
    right = shoulder - rest["upperarm.l"].head_local
    right = (right - up * right.dot(up)).normalized()
    forward = up.cross(right)
    arm_length = sum(rest[name].length for name in ("upperarm.r", "lowerarm.r", "wrist.r"))

    def hand_offset(frame):
        whole_frame = floor(frame)
        scene.frame_set(whole_frame, subframe=frame - whole_frame)
        bones = rig.evaluated_get(bpy.context.evaluated_depsgraph_get()).pose.bones
        return bones["hand.r"].head - bones["upperarm.r"].head

    def number(value):
        return f"{round(value, 5) + 0.0:.5f}f"

    lines = [
        "#pragma once",
        "",
        "// Kay Lousberg, KayKit Character Animations 1.1 (CC0 1.0).",
        "// Reproduce with Tools/Art/extract_cast_motion.py.",
        "namespace PvG::KayKit",
        "{",
        "struct FCastSample",
        "{",
        "    float Lift;",
        "    float Reach;",
        "    float Sweep;",
        "};",
        "",
        f"inline constexpr int CastSampleCount = {SAMPLE_COUNT};",
    ]
    for clip in CLIPS:
        source_name = "Spellcasting_Long" if clip == "SpellcastingLong" else clip
        action = bpy.data.actions[f"Rig_Medium|Ranged_Magic_{source_name}"]
        rig.animation_data.action = action
        rig.animation_data.action_slot = action.slots[0]
        first, last = action.frame_range
        neutral = hand_offset(first)
        lines.extend(["", f"inline constexpr FCastSample {clip}[CastSampleCount] = {{"])
        for index in range(SAMPLE_COUNT):
            frame = first + (last - first) * index / (SAMPLE_COUNT - 1)
            delta = (hand_offset(frame) - neutral) / arm_length
            values = ", ".join(number(delta.dot(axis)) for axis in (up, forward, right))
            lines.append(f"    {{{values}}},")
        lines.append("};")
    lines.extend(["}", ""])
    return "\n".join(lines)


if __name__ == "__main__":
    # Project shoulder-relative hand motion on the source bind-pose axes in
    # armature space. +lift is hip-to-chest, +sweep is anatomical right,
    # and +reach is up cross right. Subtract the first pose and divide by
    # the bind-pose arm length. Each clip spans nine evenly spaced samples.
    source, output = map(Path, sys.argv[sys.argv.index("--") + 1 :])
    output.write_text(render_header(source), encoding="utf-8", newline="\n")
