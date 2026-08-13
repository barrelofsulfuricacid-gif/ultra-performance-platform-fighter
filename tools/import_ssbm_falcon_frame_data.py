#!/usr/bin/env python3
"""Convert the pinned Falcon NTSC 1.02 frame-data dump to compact C tables."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
from typing import Any

from hsd_figatree import (
    TRACK_TRANSLATE_Y,
    TRACK_TRANSLATE_Z,
    decode_figatree,
    sample_track,
)
from ssbm_dat import ft_common_data
from ssbm_ecb_pose import (
    ECB_POINTS,
    canonical_sha256 as ecb_canonical_sha256,
    pose_f32 as ecb_pose_f32,
    semantic_payload as ecb_semantic_payload,
)


EXPECTED_CANONICAL_SHA256 = (
    "42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356"
)
EXTRACTOR_REVISION = "0b12c5cb988da3fb9b67630b1d8347e12cd91528"
DAT_READER_REVISION = "d4e6074aa26f388fccc7fe8e825761cf1c1bc7b0"
HSD_READER_REVISION = "29546ad77fdf9ebd9a9940ed44903ef309e810d6"
SOURCE_DAT_SHA256 = "4cf61a52737d464df9298fd15573345fb3b9a15c79ab47dce4fd2e3e707917af"
SOURCE_ANIMATION_DAT_SHA256 = (
    "a9a0ccc2382a2f02d5423675469719488540dd119a14577712c97348f70e1c1a"
)
SOURCE_DAT_JSON_SHA256 = (
    "fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266"
)
SOURCE_COMMON_DAT_SHA256 = (
    "63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c"
)
DOWN_BOUND_FLOOR_CONTACT_CAPTURE_SHA256 = (
    "6c8d97ff1076075616ed06f88c742528eff9c2fb18ab9f2cce09ba895147e556"
)
DAMAGE_FLY_ECB_CAPTURE_SHA256 = (
    "d011c9bb79f93840d1d97fbf241b754cedf5669c2578c9f1f7f85b45a3f6bd84"
)
DAMAGE_FLY_ECB_FULL_SEMANTIC_SHA256 = (
    "9efade94dbd61446decfabeedce910e4a2823bfc65299b7ecb4cb31fb368eee1"
)
PLATFORM_DROP_ECB_CAPTURE_SHA256 = (
    "0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c"
)
PLATFORM_DROP_ECB_SEMANTIC_SHA256 = (
    "90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8"
)
AIRBORNE_ECB_PROFILE_SHA256 = (
    "b2c423622e1794008e7c8b579dacb1b0953fcbdfd590d6b3d8111c809c54b18b"
)
AIRBORNE_ECB_CAPTURE_SHA256 = (
    "4e6768e0862307eb32a14532fae8e2991e2900ea932b7af45850803c2ec8673f"
)
AIRBORNE_ECB_SEMANTIC_SHA256 = (
    "6f9a131604c9b40d683759ab8c8eb15b946ca201ab31773cdb9354054a1478fb"
)
AERIAL_ATTACK_ECB_PROFILE_SHA256 = (
    "0a826acad85fb69612985b472f565029abbc4a3212f877eac925327e7494d11e"
)
AERIAL_ATTACK_ECB_CAPTURE_SHA256 = (
    "9978972ba84a870ae5456c2403234d837c8b425f6dde4f3df83993a809e5534d"
)
AERIAL_ATTACK_ECB_SEMANTIC_SHA256 = (
    "e4f5861010ffc5184caa7cfff58db550249ef2f4156f34e438da9a3ec8d97206"
)
SHIELD_BREAK_ECB_PROFILE_SHA256 = (
    "be0e6532a5ab350331f7f314cbdde5c217fab10d6738bef64fcabdde72c9ba1c"
)
SHIELD_BREAK_ECB_CAPTURE_SHA256 = (
    "1109c92ec4c57bff5658d25c432383ccc4c63e2caed73a1575ae3ef80c7c802d"
)
SHIELD_BREAK_ECB_SEMANTIC_SHA256 = (
    "7ad4c2d2e36b40f55cfb55e79ac764415492cae3ce648ee70b103ae6c4cf2095"
)
GUARD_ECB_PROFILE_SHA256 = (
    "08bf823fd41cb9fc606d9f5f6f6e9390cede1b0fc5996e25e0135c0d81fa2b20"
)
GUARD_ECB_CAPTURE_SHA256 = (
    "e9141d1ce253bee82233d9545cf20145d594d60510cee5ea77b19ca5e12390b9"
)
GUARD_ECB_SEMANTIC_SHA256 = (
    "789bfc726a146ad6b796a2f2d4050c8efe7c50224d4c1fcbe2c8d38a73349b1c"
)
DOWN_BOUND_ECB_PROFILE_SHA256 = (
    "8091fa5eaf8cd87fe8db3660d974e3ee7d08ebbce48af92a8e3dd00b94458b4d"
)
DOWN_BOUND_ECB_CAPTURE_SHA256 = (
    "c9bca1cb43fad6c0b6fb73c123faaeef0725b9737b84de4abf38d917386a2cfb"
)
DOWN_BOUND_ECB_SEMANTIC_SHA256 = (
    "8f42c1eea5507e089be3275f5051cd187690de08602fd8657f65dd9a66cb2432"
)
GETUP_ECB_PROFILE_SHA256 = (
    "14fb7fba797af1a5561a0d17aa5b507c18599f870096acd3360a5b72e38b943d"
)
GETUP_ECB_CAPTURE_SHA256 = (
    "22d96deaa0e2c32ce9edba670285ce6268b442edf05ae97c01abe85a93c8059c"
)
GETUP_ECB_SEMANTIC_SHA256 = (
    "af92dec06b20de798a9309170de6314206ccdf8ea629ba7af96897ebdb47f93d"
)
GROUND_LOOP_ECB_PROFILE_SHA256 = (
    "9de0d9ef89e0d578205326fb9dc56c8ba57cb3b04229b147c1e43fb4a28d2b68"
)
GROUND_LOOP_ECB_CAPTURE_SHA256 = (
    "cb07f5c3bff1f55e7f223e3863822a6d023bb6adf9ad13b69918111fcb341ba6"
)
GROUND_LOOP_ECB_SEMANTIC_SHA256 = (
    "158267ef074f57056a6be09650757d375dcc0002aeb21de4347a97c2b736a206"
)
BOUNCE_ECB_PROFILE_SHA256 = (
    "efa5bf140770c2fff8697b73f618ecb658d851c0d03361a992ec7a4ecc29f9d3"
)
BOUNCE_ECB_CAPTURE_SHA256 = (
    "f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2"
)
BOUNCE_ECB_SEMANTIC_SHA256 = (
    "fa689a969a864fcac018e7479e29d30807af189b62fa84f38ab5ae39fc5b6e60"
)
LEDGE_ROOT_CAPTURE_SHA256 = (
    "0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3"
)
LEDGE_JUMP1_HYRULE_SOURCE_TRACE_SHA256 = (
    "0882c32de5571a7fedec49d2b7e447bd46ccef930274b9603682239de57ce371"
)

# Absolute frame-one TransN positions from the live-qualified Hyrule line-37
# route. The generic translation pool stores deltas for physics callbacks;
# CliffCatch/CliffWait physics instead positions from absolute TransN relative
# to the ledge endpoint every frame.
LEDGE_CATCH_FRAME_ONE_ROOT_MELEE = (-5.906890869140625, -20.114771366119385)
LEDGE_WAIT_FRAME_ONE_ROOT_MELEE = (-2.4527130126953125, -23.096231937408447)
LEDGE_OPTION_FRAME_ONE_ROOT_MELEE = (
    (-2.03436279296875, -24.829246997833252),
    (-2.008941650390625, -24.652788639068604),
    (-2.0343017578125, -24.829304218292236),
    (-2.391510009765625, -21.520670413970947),
    (-2.4515228271484375, -23.096231937408447),
    (-2.390594482421875, -21.520813465118408),
    (-2.2660675048828125, -21.522364139556885),
    (0.0, 0.0),
    (-2.124969482421875, -21.4601788520813),
    (0.0, 0.0),
)
# First source animation frame whose Cliff* collision callback grounds the
# rooted option on the live-qualified Hyrule line-37 route. Zero means that
# phase is never grounded. Order is submotions 219 through 228.
LEDGE_OPTION_GROUND_FRAME = (37, 19, 28, 21, 37, 17, 0, 0, 0, 0)

# Post-collision world displacement from CliffWait frame one on Hyrule's
# right ledge. CliffJump1_Phys writes TransN relative to the ledge and its
# collision callback resolves the animated ECB against the ledge wall; the X
# result therefore cannot be reconstructed from the character root track
# alone. The left ledge is the exact mirror in the compact stage profile.
LEDGE_JUMP1_HYRULE_QUICK_FROM_WAIT_F32 = (
    (-0.034194946289, -0.290267944336),
    (-0.07177734375, -0.603103637695),
    (-0.111175537109, -0.930023193359),
    (-0.150848388672, -1.262573242188),
    (-0.168869018555, -1.592269897461),
    (-0.174041748047, -1.910629272461),
    (-0.166381835938, -2.209197998047),
    (-0.171508789062, -2.367904663086),
    (-0.222640991211, -2.572540283203),
    (-0.219451904297, -3.051086425781),
    (-0.225112915039, -3.616760253906),
)
LEDGE_JUMP1_HYRULE_SLOW_FROM_WAIT_F32 = (
    (-0.019470214844, -0.279235839844),
    (-0.041458129883, -0.574600219727),
    (-0.06462097168, -0.882247924805),
    (-0.087600708008, -1.198303222656),
    (-0.109069824219, -1.518905639648),
    (-0.127700805664, -1.840179443359),
    (-0.142135620117, -2.158264160156),
    (-0.151916503906, -2.490905761719),
    (-0.157852172852, -2.845504760742),
    (-0.160522460938, -3.202789306641),
    (-0.160491943359, -3.543441772461),
    (-0.15837097168, -3.848190307617),
    (-0.154724121094, -4.097717285156),
    (-0.139724731445, -4.187118530273),
    (-0.116287231445, -4.132858276367),
    (-0.104110717773, -4.097717285156),
    (-0.108795166016, -4.155075073242),
    (-0.124725341797, -4.231567382812),
)
LEDGE_JUMP2_HYRULE_FRAME_ONE_FROM_WAIT_F32 = (
    (-0.30810546875, -4.202239990234),
    (-0.229080200195, -4.817047119141),
)

COMMON_ATTRIBUTE_COUNT = 97
SUBMOTION_COUNT = 318
SPECIAL_ATTRIBUTE_SIZE = 0x8C
STALE_MOVE_SLOT_COUNT = 9

# ftCaptain_DatAttrs at doldecomp/melee revision 9509dc0. Keep the raw words
# as the source of truth; generated fields preserve their IEEE-754 binary32
# values and apply only the documented world-unit conversion.
SPECIAL_FLOAT_ATTRIBUTES = (
    "specialn_stick_range_y_neg",
    "specialn_stick_range_y_pos",
    "specialn_angle_diff",
    "specialn_vel_x",
    "specialn_vel_mul",
    "specials_gr_vel_x",
    "specials_grav",
    "specials_terminal_vel",
    "specials_unk0",
    "specials_unk1",
    "specials_unk2",
    "specials_unk3",
    "specials_unk4",
    "specials_unk5",
    "specials_miss_landing_lag",
    "specials_hit_landing_lag",
    "specialhi_air_friction_mul",
    "specialhi_horz_vel",
    "specialhi_freefall_air_spd_mul",
    "specialhi_landing_lag",
    "specialhi_unk0",
    "specialhi_unk1",
    "specialhi_input_var",
    "specialhi_unk2",
    "specialhi_catch_grav",
)

SPECIAL_TAIL_ATTRIBUTES = (
    ("specialhi_air_var", "i32", 0x64),
    ("x68", "bits", 0x68),
    ("speciallw_unk1", "u32", 0x6C),
    ("speciallw_flame_particle_angle", "f32", 0x70),
    ("speciallw_on_hit_spd_modifier", "f32", 0x74),
    ("speciallw_unk2", "i32", 0x78),
    ("speciallw_ground_lag_mul", "f32", 0x7C),
    ("speciallw_landing_lag_mul", "f32", 0x80),
    ("speciallw_ground_traction", "f32", 0x84),
    ("speciallw_air_landing_traction", "f32", 0x88),
)

SPECIAL_UNSIGNED_FIELDS = frozenset({"x68_bits", "speciallw_unk1"})

MOVE_KEYS = (
    "jab1",
    "jab2",
    "jab3",
    "rapidjabs_start",
    "rapidjabs_loop",
    "rapidjabs_end",
    "dashattack",
    "ftilt_h",
    "ftilt_mh",
    "ftilt_m",
    "ftilt_ml",
    "ftilt_l",
    "utilt",
    "dtilt",
    "fsmash_h",
    "fsmash_mh",
    "fsmash_m",
    "fsmash_ml",
    "fsmash_l",
    "usmash",
    "dsmash",
    "nair",
    "fair",
    "bair",
    "uair",
    "dair",
    "grab",
    "dashgrab",
    "pummel",
    "fthrow",
    "bthrow",
    "uthrow",
    "dthrow",
    "0x12d",
    "0x12e",
    "0x12f",
    "0x130",
    "0x131",
    "0x132",
    "0x133",
    "0x134",
    "0x135",
    "0x136",
    "0x137",
    "0x138",
    "0x139",
    "0x13a",
    "0x13b",
    "0x13c",
    "0x13d",
)

# The extractor uses the common five-angle forward-smash schema. Falcon's DAT
# has only high, straight, and low variants, so these two slots are
# intentionally absent rather than incomplete extraction results.
EXPECTED_ABSENT_MOVE_KEYS = frozenset({"fsmash_mh", "fsmash_ml"})

SMASH_MOVE_KEYS = ("fsmash_h", "fsmash_m", "fsmash_l", "usmash", "dsmash")

ELEMENTS = {
    "empty": "PF_M4_REFERENCE_HIT_EMPTY",
    "normal": "PF_M4_REFERENCE_HIT_NORMAL",
    "fire": "PF_M4_REFERENCE_HIT_FIRE",
    "electric": "PF_M4_REFERENCE_HIT_ELECTRIC",
    "grab": "PF_M4_REFERENCE_HIT_GRAB",
}

ANIMATION_TRANSLATION_FLAG = 0x80000000
ANIMATION_TRANSLATION_NODE_MASK = 0x3F
MELEE_X_TO_SIM = 12.0 / 115.0
MELEE_Y_TO_SIM = 11.0 / 62.0

# ftCa_SpecialHiThrow0 starts by running ftCo_800DE2A8. When Falcon caught a
# grounded victim, that source routine relocates the thrower from the victim's
# TransN2 attachment before animation-root motion begins. These NTSC 1.02
# offsets are the observed relocation at SpecialHiCatch frame 15 ->
# SpecialHiThrow frame 0 in the strict Dolphin capture whose SHA-256 is
# 27d869d3d9873d91690223d014cf0e7875fcd2b7138013bac1229c8512c32c60.
SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE = -10.707747459411621
SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE = 2.545643227005005

# CaptureCaptain enters damage with zero applied launch velocity, but the common
# damage state is initialized with 27 ticks and consumes one on the transition
# frame. This visible 26-tick release interval was read from fighter+0x2340 in
# the pinned NTSC 1.02 Dolphin capture (SHA-256
# 59a4489ea6e955c9bb587bb5e49bc5d34ce4cce6ae42accd98a24ff97e271a6f)
# and follows ftCo_800DE2A8 -> ftCo_800DE7C0 -> ftCo_Damage in the decomp.
SPECIALHI_CAPTURE_VICTIM_RELEASE_HITSTUN_TICKS = 26

# DownBoundU and DownBoundD both retain floor contact for displayed frames
# 1..4, lose it for frames 5..22 as their JObj-driven ECB rises, and regain it
# for frames 23..26. These complete schedules were captured independently for
# both orientations in the pinned 1,515-row prone-response oracle. The digest
# above covers the canonical pair of 26 booleans; one bit per displayed frame
# keeps the runtime table allocation-free and branch-light.
DOWN_BOUND_BACK_FLOOR_CONTACT_MASK = 0x03C0000F
DOWN_BOUND_STOMACH_FLOOR_CONTACT_MASK = 0x03C0000F

# Falcon's Falling ECB bottom is animation-derived, not the authored gameplay
# body's half-height. Captured directly at fighter+0x794 from the same NTSC
# 1.02 process (ECB capture SHA-256
# 4518dbb5cd43158baeaa1ddad7d5ffd073b4dda46ecbe2aa55d8c7efa9eadfdb).
FALLING_ECB_BOTTOM_Y_MELEE = 7.932853698730469

# Pass is a 30-frame common submotion (index 209). Its animation clock is
# independent from common-data x470, which only controls how long collision
# skips the platform that was just left. The complete frame 0..29 ECB was
# captured with the fighter relocated after Pass became active so landing
# could not truncate the animation. The raw capture and the canonical
# big-endian (u32 action frame, f32 bottom-Y) stream are pinned above.
PLATFORM_DROP_ECB_BOTTOM_Y_MELEE = (
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    6.50372314453125,
    4.010986328125,
    0.781219482421875,
    0.16302490234375,
    2.0479736328125,
    0.787353515625,
    0.23211669921875,
    0.577789306640625,
    0.728912353515625,
    0.3431396484375,
    0.202789306640625,
    0.1751708984375,
    0.2315673828125,
    0.36334228515625,
    0.560394287109375,
    0.79730224609375,
    1.043670654296875,
    1.24462890625,
    1.4481201171875,
    1.646820068359375,
    1.834503173828125,
)

# Complete displayed-frame ECB bottom for the pinned natural Hyrule
# DamageFlyN route. Frames 1-5 use the source zero-bottom fallback; the later
# pose is what defers line-37 landing until displayed frame 24.
DAMAGE_FLY_ECB_BOTTOM_Y_MELEE = (
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    7.080801963806152,
    7.315534591674805,
    7.473471164703369,
    7.564826011657715,
    7.6020355224609375,
    7.5979156494140625,
    7.564871311187744,
    7.514712333679199,
    7.458775520324707,
    7.408194065093994,
    7.37419319152832,
    7.368307113647461,
    7.402467727661133,
    7.488738059997559,
    7.595531463623047,
    7.681507110595703,
    7.744410037994385,
    7.750994682312012,
    7.681281089782715,
)

# Complete DamageFlyN ECB top and symmetric side point. Three independently
# captured Hyrule routes produce the same 24-frame float table and semantic
# digest above. Melee's wall and ceiling queries consume these points; the
# bottom-only table is insufficient for sloped Battlefield collision.
DAMAGE_FLY_ECB_TOP_Y_MELEE = (
    14.397089004516602,
    13.162859916687012,
    11.669897079467773,
    11.075756072998047,
    10.84164047241211,
    11.1769380569458,
    11.335014343261719,
    11.350427627563477,
    11.338473320007324,
    11.39617919921875,
    11.492490768432617,
    11.588090896606445,
    11.643549919128418,
    11.68921184539795,
    11.858858108520508,
    12.093215942382812,
    12.338375091552734,
    12.583917617797852,
    12.816632270812988,
    13.137887954711914,
    13.171178817749023,
    13.244199752807617,
    13.136595726013184,
    13.367762565612793,
)
DAMAGE_FLY_ECB_SIDE_X_MELEE = (
    3.7614593505859375,
    3.92529296875,
    4.276664733886719,
    4.552490234375,
    4.5292205810546875,
    4.490440368652344,
    4.4280853271484375,
    4.367012023925781,
    4.300819396972656,
    4.229911804199219,
    4.154304504394531,
    4.0759735107421875,
    4.0001983642578125,
    3.925323486328125,
    3.8440704345703125,
    3.7541122436523438,
    3.6508026123046875,
    3.58642578125,
    3.5352783203125,
    3.3623733520507812,
    3.1258544921875,
    2.8462753295898438,
    3.7246017456054688,
    4.9794464111328125,
)
DAMAGE_FLY_ECB_SIDE_Y_MELEE = (
    9.784567832946777,
    10.077530860900879,
    9.982057571411133,
    9.595584869384766,
    8.790365219116211,
    9.128870010375977,
    9.325274467468262,
    9.411949157714844,
    9.45164966583252,
    9.499107360839844,
    9.54520320892334,
    9.576480865478516,
    9.579131126403809,
    9.573993682861328,
    9.633525848388672,
    9.733704566955566,
    9.853341102600098,
    9.993192672729492,
    10.152685165405273,
    10.36670970916748,
    10.426342964172363,
    10.494304656982422,
    10.443795204162598,
    10.524521827697754,
)

# EscapeAir displayed frames 1 through 48 from the pinned defense-state
# capture SHA-256 1118a7a6e26ae98862e7457caee59ff45260076f30ce2e3e09ba71f249dc6084.
# The final two airborne samples are what defer floor contact until Melee's
# displayed frame 49; substituting Falcon's standing extent lands one frame
# early. Frames 1 through 7 expose the source ECB's zero-bottom fallback.
AIR_DODGE_ECB_BOTTOM_Y_MELEE = (
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    1.9960927963256836,
    1.9618406295776367,
    1.9201927185058594,
    1.872711181640625,
    1.8209609985351562,
    1.7664909362792969,
    1.7107677459716797,
    1.6551704406738281,
    1.600931167602539,
    1.54913330078125,
    1.5006866455078125,
    1.4557933807373047,
    1.413930892944336,
    1.374969482421875,
    1.338998794555664,
    1.306365966796875,
    1.277730941772461,
    1.2541141510009766,
    1.2452125549316406,
    1.2524890899658203,
    1.2624645233154297,
    1.2417945861816406,
    1.1859264373779297,
    1.0185871124267578,
    0.7817115783691406,
    0.6812477111816406,
    0.6629657745361328,
    0.7087917327880859,
    0.7791938781738281,
    0.8813133239746094,
    1.3754844665527344,
    1.1521835327148438,
    0.8825883865356445,
    0.902409553527832,
    1.2359085083007812,
    1.5895023345947266,
    1.9857282638549805,
    1.8570032119750977,
    1.821122169494629,
    1.967071533203125,
    2.132474660873413,
)

# SpecialLw frame 39 -> SpecialLwEnd frame 1 retains exactly 80% of the
# animation-driven ground velocity. This is observed in the pinned vanilla
# NTSC 1.02 down-ground capture (SHA-256
# 6244baaf1354749a118a3577f3ca080f87dc4ba59d60f14b947077922a667a2d),
# rather than inferred from a hand-authored tuning constant.
FALCON_KICK_GROUND_END_ENTRY_VELOCITY_SCALE = 0.8


def canonical_sha256(data: dict[str, Any]) -> str:
    encoded = json.dumps(data, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def binary32(value: float) -> float:
    """Round once to the runtime's IEEE-754 binary32 representation."""

    return struct.unpack(">f", struct.pack(">f", float(value)))[0]


def c_f32(value: float) -> str:
    """Render a finite, round-trippable C binary32 literal."""

    rounded = binary32(value)
    if not math.isfinite(rounded):
        raise ValueError("non-finite generated float")
    rendered = format(rounded, ".9g")
    if "e" not in rendered.lower() and "." not in rendered:
        rendered += ".0"
    return rendered + "f"


def captured_fixed_f32(value: int | float) -> float:
    """Decode legacy stored ECB capture units while profiles are regenerated."""

    return binary32(float(value) / 65536.0) if isinstance(value, int) else binary32(value)


def render_ecb_pose_f32(frame: dict[str, Any]) -> str:
    ecb = frame["ecb_f32"]
    values = [
        value
        for point in ECB_POINTS
        for value in ecb[point]
    ]
    return "{ " + ", ".join(c_f32(captured_fixed_f32(value)) for value in values) + " }"


def render_ecb_pose_track(
    frames: tuple[dict[str, Any], ...], indentation: str
) -> list[str]:
    return [
        f"{indentation}{{",
        *(f"{indentation}    {render_ecb_pose_f32(frame)}," for frame in frames),
        f"{indentation}}},",
    ]


def render_ecb_pose_track_table(
    tracks: tuple[tuple[dict[str, Any], ...], ...], indentation: str
) -> list[str]:
    return [
        line
        for frames in tracks
        for line in render_ecb_pose_track(frames, indentation)
    ]


def render_ecb_pose_track_matrix(
    tracks: tuple[tuple[tuple[dict[str, Any], ...], ...], ...],
    indentation: str,
) -> list[str]:
    lines: list[str] = []
    for row in tracks:
        lines.append(f"{indentation}{{")
        lines.extend(render_ecb_pose_track_table(row, indentation + "    "))
        lines.append(f"{indentation}}},")
    return lines


def load_ecb_profile(
    path: Path,
    *,
    expected_profile_sha256: str,
    expected_capture_sha256: str,
    expected_semantic_sha256: str,
    expected_tracks: tuple[tuple[str, str, int, int], ...],
) -> dict[str, Any]:
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != expected_profile_sha256:
        raise ValueError(f"unexpected ECB profile SHA-256: {digest}")
    profile = json.loads(raw)
    if (
        profile.get("schema") != 2
        or profile.get("scope") != "ssbm-action-owned-ecb-pose-tracks"
        or profile.get("capture_sha256") != expected_capture_sha256
        or profile.get("semantic_sha256") != expected_semantic_sha256
        or profile.get("coordinate_conversion")
        != {
            "rounding": "ieee754-binary32",
            "x_sim_units_per_melee_unit": "12/115",
            "y_sim_units_per_melee_unit": "11/62",
        }
    ):
        raise ValueError("unexpected ECB profile provenance")
    tracks = profile.get("tracks")
    if not isinstance(tracks, list) or len(tracks) != len(expected_tracks):
        raise ValueError("incomplete ECB profile")
    for track, (track_id, source_action, first_displayed_frame, frame_count) in zip(
        tracks, expected_tracks, strict=True
    ):
        if (
            not isinstance(track, dict)
            or track.get("id") != track_id
            or track.get("source_action") != source_action
            or track.get("canonical_facing") != 1
            or track.get("first_displayed_frame", 0) != first_displayed_frame
            or track.get("frame_count") != frame_count
            or not isinstance(track.get("frames"), list)
            or len(track["frames"]) != frame_count
        ):
            raise ValueError(f"invalid ECB track {track_id!r}")
        for frame_index, frame in enumerate(track["frames"]):
            displayed_frame = first_displayed_frame + frame_index
            source_ecb = frame.get("source_ecb")
            stored_ecb = frame.get("ecb_f32")
            if (
                not isinstance(frame, dict)
                or frame.get("displayed_frame") != displayed_frame
                or not isinstance(source_ecb, dict)
                or not isinstance(stored_ecb, dict)
                or set(source_ecb) != set(ECB_POINTS)
                or set(stored_ecb) != set(ECB_POINTS)
                or ecb_pose_f32(source_ecb) != stored_ecb
                or any(
                    not isinstance(value, (int, float))
                    or isinstance(value, bool)
                    or not math.isfinite(float(value))
                    for point in ECB_POINTS
                    for value in stored_ecb[point]
                )
            ):
                raise ValueError(
                    f"invalid ECB pose {track_id}:{displayed_frame}"
                )
    actual_semantic = ecb_canonical_sha256(ecb_semantic_payload(tracks))
    if actual_semantic != expected_semantic_sha256:
        raise ValueError(
            f"unexpected ECB semantic SHA-256: {actual_semantic}"
        )
    return profile


def hash_figatree(
    digest: Any,
    submotion_index: int,
    animation_size: int,
    tree: Any | None,
) -> tuple[int, int, int]:
    """Hash every decoded node, track, and key in canonical source order."""

    nodes = () if tree is None else tree.nodes
    frame_count = 0.0 if tree is None else float(tree.frame_count)
    digest.update(
        struct.pack(
            ">HIdH",
            submotion_index,
            animation_size,
            frame_count,
            len(nodes),
        )
    )
    track_count = 0
    key_count = 0
    for node_index, node in enumerate(nodes):
        digest.update(struct.pack(">HH", node_index, len(node)))
        track_count += len(node)
        for track in node:
            digest.update(
                struct.pack(
                    ">BiI",
                    int(track.track_type),
                    int(track.start_frame),
                    len(track.keys),
                )
            )
            key_count += len(track.keys)
            for key in track.keys:
                digest.update(
                    struct.pack(
                        ">dddB",
                        float(key.frame),
                        float(key.value),
                        float(key.tangent),
                        int(key.interpolation),
                    )
                )
    return len(nodes), track_count, key_count


def animation_translation_node(action_flags: int) -> int | None:
    """Decode HSD's one-based TransN node field from packed action flags."""

    if not action_flags & ANIMATION_TRANSLATION_FLAG:
        return None
    encoded_node = action_flags & ANIMATION_TRANSLATION_NODE_MASK
    if encoded_node == 0:
        raise ValueError(
            "animation-translation flag has no encoded TransN node"
        )
    return encoded_node - 1


def animation_translation_f32(
    tree: Any,
    action_flags: int,
    model_scaling: float,
) -> tuple[list[int], list[int]]:
    """Sample one source TransN delta per displayed gameplay frame.

    The packed node field is six bits wide. Bits 6 and 7 have independent
    action semantics, so treating the entire low byte as the node number
    breaks animations such as EscapeF (0x800000c2).
    """

    translation_node = animation_translation_node(action_flags)
    if translation_node is None:
        return [], []
    if not 0 <= translation_node < len(tree.nodes):
        raise ValueError(f"invalid translation node {translation_node}")

    translation_x_tracks = [
        track
        for track in tree.nodes[translation_node]
        if track.track_type == TRACK_TRANSLATE_Z
    ]
    translation_y_tracks = [
        track
        for track in tree.nodes[translation_node]
        if track.track_type == TRACK_TRANSLATE_Y
    ]
    if len(translation_x_tracks) > 1:
        raise ValueError(
            "expected at most one translation-Z track, "
            f"found {len(translation_x_tracks)}"
        )
    if len(translation_y_tracks) > 1:
        raise ValueError(
            "expected at most one translation-Y track, "
            f"found {len(translation_y_tracks)}"
        )

    frame_count = round(tree.frame_count)
    positions_x = (
        [
            sample_track(translation_x_tracks[0], float(frame))
            for frame in range(frame_count)
        ]
        if translation_x_tracks
        else [0.0] * frame_count
    )
    positions_y = (
        [
            sample_track(translation_y_tracks[0], float(frame))
            for frame in range(frame_count)
        ]
        if translation_y_tracks
        else [0.0] * frame_count
    )
    return (
        [
            binary32(
                (positions_x[frame] - positions_x[frame - 1])
                * model_scaling
                * MELEE_X_TO_SIM
            )
            for frame in range(1, frame_count)
        ],
        [
            binary32(
                -(positions_y[frame] - positions_y[frame - 1])
                * model_scaling
                * MELEE_Y_TO_SIM
            )
            for frame in range(1, frame_count)
        ],
    )


def body_collision_timing(subaction: dict[str, Any]) -> tuple[int, int]:
    """Decode the first state-2 interval without assigning gameplay meaning."""

    frame = 0
    state_two_frame: int | None = None
    state_zero_frame: int | None = None
    for event in subaction.get("events", []):
        command_id = int(str(event["commandId"]), 16)
        encoded = bytes.fromhex(str(event["bytes"]))
        argument = int.from_bytes(encoded, "big") & 0xFFFFFF
        if command_id == 0x08:
            frame = argument
        elif command_id == 0x04:
            frame += argument
        elif command_id == 0x68:
            state = argument
            if state == 2 and state_two_frame is None:
                state_two_frame = frame
            elif state == 0 and state_two_frame is not None:
                if state_zero_frame is None:
                    state_zero_frame = frame
            elif state != 0:
                raise ValueError(
                    "unsupported body-collision command sequence in "
                    f"{subaction.get('shortName', '<unnamed>')}"
                )
    if state_two_frame is None:
        if state_zero_frame is not None:
            raise ValueError("body-collision restore has no state-2 command")
        return 0xFFFF, 0xFFFF
    if state_two_frame > 0xFFFE or (
        state_zero_frame is not None and state_zero_frame > 0xFFFE
    ):
        raise ValueError("body-collision command frame exceeds uint16_t")
    return state_two_frame, 0xFFFF if state_zero_frame is None else state_zero_frame


def u16(value: Any) -> int:
    return 0 if value is None else int(value)


def c_hit_effect(effect: dict[str, Any]) -> str:
    element = ELEMENTS[str(effect["element"])]
    return (
        "{ "
        f"UINT16_C({int(effect['angle'])}), "
        f"UINT16_C({int(effect['kbGrowth'])}), "
        f"UINT16_C({int(effect['weightDepKb'])}), "
        f"UINT16_C({int(effect['baseKb'])}), "
        f"UINT8_C({int(effect['damage'])}), "
        f"UINT8_C({int(effect['shieldDamage'])}), "
        f"UINT8_C({int(effect['hitboxInteraction'])}), "
        f"(uint8_t){element}, "
        f"UINT8_C({1 if effect['hitGrounded'] else 0}), "
        f"UINT8_C({1 if effect['hitAirborne'] else 0}), "
        "{ UINT8_C(0), UINT8_C(0) } }"
    )


def raw_f32(words: list[int], index: int) -> float:
    return struct.unpack(">f", words[index].to_bytes(4, "big"))[0]


def fighter_data_offsets(source_dat: bytes) -> tuple[int, int, int]:
    """Return ftDataCaptain's common, extended, and collision-data offsets."""

    if len(source_dat) < 0x20:
        raise ValueError("truncated PlCa.dat header")
    data_size, relocation_count, root_count, reference_count = struct.unpack_from(
        ">4I", source_dat, 0x04
    )
    if root_count != 1 or reference_count != 0:
        raise ValueError("unexpected PlCa.dat root/reference table")
    root_table = 0x20 + data_size + relocation_count * 4
    if root_table + 8 > len(source_dat):
        raise ValueError("PlCa.dat root table is out of bounds")
    root_offset, root_name_offset = struct.unpack_from(">2I", source_dat, root_table)
    string_table = root_table + root_count * 8 + reference_count * 8
    name_start = string_table + root_name_offset
    name_end = source_dat.find(b"\0", name_start)
    if name_end < 0 or source_dat[name_start:name_end] != b"ftDataCaptain":
        raise ValueError("unexpected PlCa.dat root name")
    data = source_dat[0x20 : 0x20 + data_size]
    if root_offset + 8 > len(data):
        raise ValueError("ftDataCaptain root is out of bounds")
    common_offset, special_offset = struct.unpack_from(">2I", data, root_offset)
    collision_offset = struct.unpack_from(">I", data, root_offset + 0x44)[0]
    if common_offset + COMMON_ATTRIBUTE_COUNT * 4 > len(data):
        raise ValueError("Falcon common attributes are out of bounds")
    if special_offset + SPECIAL_ATTRIBUTE_SIZE > len(data):
        raise ValueError("Falcon special attributes are out of bounds")
    if collision_offset + 0x1C > len(data):
        raise ValueError("Falcon collision attributes are out of bounds")
    return common_offset, special_offset, collision_offset


def source_attributes(source_dat: bytes) -> tuple[list[int], dict[str, int]]:
    """Decode every common word and every decomp-defined Falcon special."""

    common_offset, special_offset, _ = fighter_data_offsets(source_dat)
    data = source_dat[0x20:]
    common_bits = list(struct.unpack_from(">97I", data, common_offset))
    special: dict[str, int] = {}
    for index, name in enumerate(SPECIAL_FLOAT_ATTRIBUTES):
        value = struct.unpack_from(">f", data, special_offset + index * 4)[0]
        special[f"{name}_f32"] = binary32(value)
    for name, kind, offset in SPECIAL_TAIL_ATTRIBUTES:
        if kind == "f32":
            value = struct.unpack_from(">f", data, special_offset + offset)[0]
            special[f"{name}_f32"] = binary32(value)
        elif kind == "i32":
            special[name] = struct.unpack_from(">i", data, special_offset + offset)[0]
        elif kind == "u32":
            special[name] = struct.unpack_from(">I", data, special_offset + offset)[0]
        elif kind == "bits":
            special[f"{name}_bits"] = struct.unpack_from(
                ">I", data, special_offset + offset
            )[0]
        else:
            raise AssertionError(f"unsupported special attribute kind {kind}")
    if len(common_bits) != COMMON_ATTRIBUTE_COUNT or len(special) != 35:
        raise ValueError("incomplete Falcon attribute decode")
    return common_bits, special


def source_collision_attributes(source_dat: bytes) -> dict[str, float]:
    """Decode Falcon's complete ftData_x44 collision/ledge-snap block."""

    _, _, collision_offset = fighter_data_offsets(source_dat)
    data = source_dat[0x20:]
    joints_and_scale = struct.unpack_from(">6h4f", data, collision_offset)
    return {
        "ecb_top_joint": joints_and_scale[0],
        "ecb_bottom_joint": joints_and_scale[1],
        "ecb_right_joint": joints_and_scale[2],
        "ecb_left_joint": joints_and_scale[3],
        "ecb_transn_joint": joints_and_scale[4],
        "ecb_joint_5": joints_and_scale[5],
        "ecb_minimum_f32": binary32(joints_and_scale[6]),
        "ledge_snap_x": joints_and_scale[7],
        "ledge_snap_y": joints_and_scale[8],
        "ledge_snap_height": joints_and_scale[9],
    }


def source_common_data(common_dat: bytes) -> tuple[bytes, tuple[int, ...]]:
    """Return the pinned ftCommonData block and its root pointer table."""

    return ft_common_data(common_dat)


def source_common_special_attributes(common_dat: bytes) -> dict[str, Any]:
    """Decode the common-data fields used by Falcon's runtime simulation."""

    data, common_offsets = source_common_data(common_dat)
    common_offset = common_offsets[0]
    if common_offset + 0x25C > len(data):
        raise ValueError("ftCommonData is out of bounds")
    stale_move_offset = common_offsets[3]
    if stale_move_offset + STALE_MOVE_SLOT_COUNT * 4 > len(data):
        raise ValueError("stale-move reduction table is out of bounds")
    return {
        "fast_ground_friction_multiplier_f32": binary32(
            struct.unpack_from(">f", data, common_offset + 0x6C)[0]
        ),
        "air_drift_over_maximum_deceleration_f32": binary32(
            struct.unpack_from(">f", data, common_offset + 0x1FC)[0]
            * MELEE_X_TO_SIM
        ),
        "side_special_stick_threshold_f32": binary32(
            struct.unpack_from(">f", data, common_offset + 0x218)[0]
        ),
        "side_special_turn_threshold_f32": binary32(
            struct.unpack_from(">f", data, common_offset + 0x220)[0]
        ),
        "air_drift_dead_zone_f32": binary32(
            struct.unpack_from(">f", data, common_offset + 0x258)[0]
        ),
        "stale_move_slot_reduction_f32": [
            binary32(struct.unpack_from(">f", data, stale_move_offset + index * 4)[0])
            for index in range(STALE_MOVE_SLOT_COUNT)
        ],
    }


def source_air_dodge_attributes(common_dat: bytes) -> dict[str, int]:
    """Decode EscapeAir's common attributes with simulator-unit conversion."""

    data, common_offsets = source_common_data(common_dat)
    common_offset = common_offsets[0]
    if common_offset + 0x348 > len(data):
        raise ValueError("ftCommonData EscapeAir attributes are out of bounds")
    dead_zone_x, dead_zone_y = struct.unpack_from(
        ">2f", data, common_offset + 0x32C
    )
    item_throw_window_ticks = struct.unpack_from(
        ">i", data, common_offset + 0x334
    )[0]
    force, decay = struct.unpack_from(">2f", data, common_offset + 0x338)
    if dead_zone_x != dead_zone_y or not 0.0 < dead_zone_x < 1.0:
        raise ValueError("unexpected asymmetric EscapeAir dead zone")
    if item_throw_window_ticks < 0 or item_throw_window_ticks > 0xFFFF:
        raise ValueError("EscapeAir item-throw window does not fit runtime timing")
    return {
        # EscapeAir physics runs on the entry frame. Store the first visible
        # velocity so the runtime stores the source operation at binary32.
        "initial_velocity_x_f32": binary32(
            force * decay * MELEE_X_TO_SIM
        ),
        "initial_velocity_y_f32": binary32(
            force * decay * MELEE_Y_TO_SIM
        ),
        "decay_f32": binary32(decay),
        "dead_zone": round(dead_zone_x * 32767.0),
        "item_throw_window_ticks": item_throw_window_ticks,
    }


def command_variable_assignments(
    subactions: list[dict[str, Any]], subaction_index: int
) -> dict[tuple[int, int], int]:
    """Return exact displayed frames for action command-variable writes."""

    frame = 0
    assignments: dict[tuple[int, int], int] = {}
    for event in subactions[subaction_index].get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        command = bytes.fromhex(str(event["bytes"]))
        if command_id == 0x4C and len(command) == 4:
            variable_index = command[0] & 0x03
            value = int.from_bytes(command[1:4], "big")
            key = (variable_index, value)
            if key in assignments:
                raise ValueError(
                    f"subaction {subaction_index}: duplicate command-variable "
                    f"assignment {key}"
                )
            assignments[key] = frame
    return assignments


def throw_release_frame(
    dat_data: dict[str, Any], subaction_index: int, subaction_name: str
) -> int:
    """Read the release flag's exact action-script frame.

    meleeDat2Json calls opcode 0x14 (encoded first byte 0x50)
    ``reverseDirection``. The NTSC 1.02 decomp dispatches that opcode to
    ftAction_800718A4: argument zero raises throw_flags_b3, which
    ftCo_800DD724 consumes to release the victim. Argument one reverses
    facing instead. Reading the encoded argument avoids relying on the
    upstream display label.
    """
    subactions = dat_data["nodes"][0]["data"]["subactions"]
    subaction = subactions[subaction_index]
    if str(subaction["name"]) != subaction_name:
        raise ValueError(f"subaction {subaction_index}: name mismatch")

    frame = 0
    releases: list[int] = []
    for event in subaction.get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        elif command_id == 0x50:
            encoded = bytes.fromhex(str(event["bytes"]))
            argument = int.from_bytes(encoded, "big") & ((1 << 26) - 1)
            if argument == 0:
                releases.append(frame)
    if len(releases) != 1:
        raise ValueError(
            f"subaction {subaction_index}: expected one throw release, "
            f"found {releases}"
        )
    return releases[0]


def throw_absolute_effect(
    dat_data: dict[str, Any], subaction_index: int, subaction_name: str
) -> dict[str, int]:
    """Decode the thrower's absolute hit directly from opcode 0x22/0x88.

    The pinned meleeDat2Json throw layout predates the matched command struct:
    it decodes the final command word several bits late, turning Falcon's real
    throw BKB values 45/30/70/75 into 11/7/17/18 and interpreting padding as
    an element.  ``ftAction_80071E04`` and ``lb/types.h`` are authoritative:
    the three big-endian words after the six-bit opcode are damage, then
    angle/KBG/WDSK, then BKB/element.  Slot zero is the victim throw hit; slot
    one is the collateral thrown-body hitbox.
    """
    subactions = dat_data["nodes"][0]["data"]["subactions"]
    subaction = subactions[subaction_index]
    if str(subaction["name"]) != subaction_name:
        raise ValueError(f"subaction {subaction_index}: name mismatch")

    effects: list[dict[str, int]] = []
    for event in subaction.get("events", []):
        if int(str(event["commandId"]), 16) != 0x88:
            continue
        encoded = bytes.fromhex(str(event["bytes"]))
        if len(encoded) != 12:
            raise ValueError(
                f"subaction {subaction_index}: invalid throw command width"
            )
        word0, word1, word2 = struct.unpack(">III", encoded)
        slot = (word0 >> 23) & 0x7
        if slot != 0:
            continue
        effects.append(
            {
                "damage": word0 & 0x7FFFFF,
                "angle": word1 >> 23,
                "kbGrowth": (word1 >> 14) & 0x1FF,
                "weightDepKb": (word1 >> 5) & 0x1FF,
                "baseKb": word2 >> 23,
                "element": (word2 >> 19) & 0xF,
            }
        )
    if len(effects) != 1:
        raise ValueError(
            f"subaction {subaction_index}: expected one slot-zero throw hit, "
            f"found {len(effects)}"
        )
    return effects[0]


def ledge_attack_reference(
    subactions: list[dict[str, Any]],
    submotion_catalog: list[dict[str, int]],
    submotion_index: int,
) -> dict[str, Any]:
    """Collapse one CliffAttack script into a zero-cost runtime phase."""

    subaction = subactions[submotion_index]
    frame = 0
    first_frame: int | None = None
    end_frame: int | None = None
    effects: list[dict[str, Any]] = []
    for event in subaction.get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        elif command_id == 0x2C:
            if first_frame is None:
                first_frame = frame
            elif frame != first_frame:
                raise ValueError(
                    f"submotion {submotion_index}: disjoint CliffAttack hitboxes"
                )
            effects.append(
                {
                    key: fields[key]
                    for key in (
                        "damage",
                        "angle",
                        "kbGrowth",
                        "weightDepKb",
                        "hitboxInteraction",
                        "baseKb",
                        "element",
                        "shieldDamage",
                        "hitGrounded",
                        "hitAirborne",
                    )
                }
            )
        elif command_id == 0x40 and first_frame is not None:
            if end_frame is not None:
                raise ValueError(
                    f"submotion {submotion_index}: repeated CliffAttack clear"
                )
            end_frame = frame
    if first_frame is None or end_frame is None or end_frame <= first_frame:
        raise ValueError(
            f"submotion {submotion_index}: incomplete CliffAttack phase"
        )
    if not effects or any(effect != effects[0] for effect in effects[1:]):
        raise ValueError(
            f"submotion {submotion_index}: non-uniform CliffAttack effects"
        )
    total_frames = submotion_catalog[submotion_index]["gameplay_frame_count"]
    if end_frame > total_frames:
        raise ValueError(
            f"submotion {submotion_index}: CliffAttack exceeds animation"
        )
    return {
        "submotion": submotion_index,
        "total_frames": total_frames,
        "first_frame": first_frame,
        "last_frame": end_frame - 1,
        "effect": effects[0],
    }


def generate(
    data: dict[str, Any],
    dat_data: dict[str, Any],
    source_dat: bytes,
    animation_dat: bytes,
    common_dat: bytes,
    bounce_ecb_profile: dict[str, Any],
    airborne_ecb_profile: dict[str, Any],
    aerial_attack_ecb_profile: dict[str, Any],
    shield_break_ecb_profile: dict[str, Any],
    guard_ecb_profile: dict[str, Any],
    down_bound_ecb_profile: dict[str, Any],
    getup_ecb_profile: dict[str, Any],
    ground_loop_ecb_profile: dict[str, Any],
) -> str:
    phases: list[tuple[int, int, int]] = []
    effects: list[dict[str, Any]] = []
    throws: list[dict[str, Any]] = []
    moves: list[dict[str, int]] = []
    motion_x_f32: list[float] = []
    motion_y_f32: list[float] = []
    bounce_tracks = {
        str(track["id"]): track for track in bounce_ecb_profile["tracks"]
    }
    ceiling_bounce_frames = bounce_tracks["ceiling_bounce"]["frames"]
    wall_bounce_frames = bounce_tracks["wall_bounce"]["frames"]
    airborne_tracks = {
        str(track["id"]): track for track in airborne_ecb_profile["tracks"]
    }
    airborne_frames = tuple(
        frame
        for track_id in (
            "jump_forward",
            "jump_backward",
            "jump_aerial_forward",
            "jump_aerial_backward",
            "fall",
            "fall_aerial",
        )
        for frame in airborne_tracks[track_id]["frames"]
    )
    aerial_attack_tracks = {
        str(track["id"]): track
        for track in aerial_attack_ecb_profile["tracks"]
    }
    shield_break_tracks = {
        str(track["id"]): track
        for track in shield_break_ecb_profile["tracks"]
    }
    shield_break_fly_frames = tuple(
        shield_break_tracks["shield-break-fly"]["frames"]
    )
    shield_break_down_down_frames = tuple(
        shield_break_tracks["shield-break-down-down"]["frames"]
    )
    shield_break_stand_down_frames = tuple(
        shield_break_tracks["shield-break-stand-down"]["frames"]
    )
    shield_break_stun_frames = tuple(
        shield_break_tracks["shield-break-stun"]["frames"]
    )
    guard_tracks = {
        str(track["id"]): track for track in guard_ecb_profile["tracks"]
    }
    guard_on_frames = tuple(guard_tracks["guard_on"]["frames"])
    guard_frames = tuple(guard_tracks["guard"]["frames"])
    guard_off_frames = tuple(guard_tracks["guard_off"]["frames"])
    down_bound_tracks = {
        str(track["id"]): track for track in down_bound_ecb_profile["tracks"]
    }
    down_bound_back_frames = tuple(
        down_bound_tracks["down_bound_back"]["frames"]
    )
    down_bound_stomach_frames = tuple(
        down_bound_tracks["down_bound_stomach"]["frames"]
    )
    getup_tracks = {
        str(track["id"]): track for track in getup_ecb_profile["tracks"]
    }
    ground_loop_tracks = {
        str(track["id"]): track for track in ground_loop_ecb_profile["tracks"]
    }
    crouch_wait_frames = tuple(ground_loop_tracks["crouch_wait"]["frames"])

    def getup_frames(track_id: str) -> tuple[dict[str, Any], ...]:
        return tuple(getup_tracks[track_id]["frames"])

    down_wait_frames = (
        getup_frames("down_wait_back"),
        getup_frames("down_wait_stomach"),
    )
    getup_neutral_frames = (
        getup_frames("getup_neutral_back"),
        getup_frames("getup_neutral_stomach"),
    )
    getup_attack_frames = (
        getup_frames("getup_attack_back"),
        getup_frames("getup_attack_stomach"),
    )
    getup_roll_frames = (
        (
            getup_frames("getup_roll_forward_back"),
            getup_frames("getup_roll_backward_back"),
        ),
        (
            getup_frames("getup_roll_forward_stomach"),
            getup_frames("getup_roll_backward_stomach"),
        ),
    )

    fighter_data = dat_data["nodes"][0]["data"]
    subactions = fighter_data["subactions"]
    subactions_offset = int(fighter_data["subactionsOffset"])
    attributes = {
        str(attribute["name"]): attribute["value"]
        for attribute in fighter_data["attributes"]
    }
    model_scaling = float(attributes["modelScaling"])
    source_dat_block = source_dat[0x20:]
    if len(subactions) != SUBMOTION_COUNT:
        raise ValueError(
            f"incomplete Falcon submotion catalog: {len(subactions)}"
        )

    smash_charge_rows: list[tuple[int, int]] = []
    for key in SMASH_MOVE_KEYS:
        move = data[key]
        if move is None:
            raise ValueError(f"{key}: missing smash-charge source move")
        subaction = subactions[int(move["subactionIndex"])]
        commands = [
            bytes.fromhex(str(event["bytes"]))
            for event in subaction["events"]
            if int(str(event["commandId"]), 16) == 0xE0
        ]
        unique_commands = set(commands)
        if (
            not commands
            or len(unique_commands) != 1
            or len(next(iter(unique_commands))) != 8
        ):
            raise ValueError(
                f"{key}: expected one unique complete Smash Charge command"
            )
        command_word = struct.unpack_from(">I", next(iter(unique_commands)), 0)[0]
        smash_charge_rows.append(
            ((command_word >> 16) & 0x3FF, command_word & 0xFFFF)
        )
    if len(set(smash_charge_rows)) != 1:
        raise ValueError(
            f"Falcon smash moves disagree on charge command: {smash_charge_rows}"
        )
    smash_charge_ticks, smash_charge_damage_multiplier_q8 = smash_charge_rows[0]
    if (
        smash_charge_ticks != 60
        or smash_charge_damage_multiplier_q8 != 350
    ):
        raise ValueError(
            "unexpected Falcon Smash Charge parameters: "
            f"ticks={smash_charge_ticks} "
            f"damage_q8={smash_charge_damage_multiplier_q8}"
        )
    if subactions_offset + SUBMOTION_COUNT * 0x18 > len(source_dat_block):
        raise ValueError("Falcon submotion records are out of bounds")
    submotion_catalog: list[dict[str, int]] = []
    script_events: list[dict[str, int]] = []
    script_bytes = bytearray()
    body_collision_timings: list[tuple[int, int]] = []
    animation_tracks_digest = hashlib.sha256()
    animation_node_count = 0
    animation_track_count = 0
    animation_key_count = 0
    for submotion_index, subaction in enumerate(subactions):
        animation_size = int(subaction["animSize"])
        animation_frame_count = 0
        tree = None
        if animation_size != 0:
            animation_offset = int(subaction["animOffset"])
            tree = decode_figatree(
                animation_dat[animation_offset : animation_offset + animation_size]
            )
            animation_frame_count = round(tree.frame_count)
            if (
                animation_frame_count <= 0
                or float(animation_frame_count) != tree.frame_count
                or animation_frame_count > 0xFFFF
            ):
                raise ValueError(
                    f"submotion {submotion_index}: invalid frame count "
                    f"{tree.frame_count}"
                )
        node_count, track_count, key_count = hash_figatree(
            animation_tracks_digest,
            submotion_index,
            animation_size,
            tree,
        )
        animation_node_count += node_count
        animation_track_count += track_count
        animation_key_count += key_count
        animation_flags = struct.unpack_from(
            ">I",
            source_dat_block,
            subactions_offset + submotion_index * 0x18 + 0x10,
        )[0]
        motion_offset = len(motion_x_f32)
        if animation_flags & ANIMATION_TRANSLATION_FLAG:
            if tree is None:
                raise ValueError(
                    f"submotion {submotion_index}: translation without animation"
                )
            submotion_motion_x_f32, submotion_motion_y_f32 = (
                animation_translation_f32(
                    tree,
                    animation_flags,
                    model_scaling,
                )
            )
            if (
                len(submotion_motion_x_f32) != animation_frame_count - 1
                or len(submotion_motion_y_f32) != animation_frame_count - 1
            ):
                raise ValueError(
                    f"submotion {submotion_index}: incomplete translation samples"
                )
            motion_x_f32.extend(submotion_motion_x_f32)
            motion_y_f32.extend(submotion_motion_y_f32)
        motion_count = len(motion_x_f32) - motion_offset
        if len(motion_x_f32) != len(motion_y_f32):
            raise ValueError(
                f"submotion {submotion_index}: mismatched translation samples"
            )
        if motion_offset > 0xFFFF or motion_count > 0xFFFF:
            raise ValueError("Falcon translation table exceeds compact offsets")
        event_count = len(subaction["events"])
        if event_count > 0xFFFF:
            raise ValueError(f"submotion {submotion_index}: too many events")
        event_offset = len(script_events)
        if event_offset > 0xFFFF:
            raise ValueError("Falcon action-script event table is too large")
        for event_index, event in enumerate(subaction["events"]):
            encoded = bytes.fromhex(str(event["bytes"]))
            byte_count = int(event["length"])
            command_id = int(str(event["commandId"]), 16)
            if (
                byte_count == 0
                or byte_count != len(encoded)
                or byte_count % 4 != 0
                or byte_count > 0xFF
            ):
                raise ValueError(
                    f"submotion {submotion_index} event {event_index}: "
                    "invalid encoded length"
                )
            if command_id > 0xFF or encoded[0] & 0xFC != command_id:
                raise ValueError(
                    f"submotion {submotion_index} event {event_index}: "
                    "opcode does not match encoded command"
                )
            byte_offset = len(script_bytes)
            if byte_offset > 0xFFFF:
                raise ValueError("Falcon action-script byte table is too large")
            script_events.append(
                {
                    "byte_offset": byte_offset,
                    "byte_count": byte_count,
                    "command_id": command_id,
                }
            )
            script_bytes.extend(encoded)
        submotion_catalog.append(
            {
                "animation_frame_count": animation_frame_count,
                "gameplay_frame_count": max(0, animation_frame_count - 1),
                "event_count": event_count,
                "event_offset": event_offset,
                "animation_flags": animation_flags,
                "animation_size": animation_size,
                "motion_offset": motion_offset,
                "motion_count": motion_count,
            }
        )
        body_collision_timings.append(body_collision_timing(subaction))
    ledge_attacks = [
        ledge_attack_reference(
            subactions,
            submotion_catalog,
            submotion_index,
        )
        for submotion_index in (
            221,
            222,
        )
    ]
    if (
        sum(row["animation_frame_count"] != 0 for row in submotion_catalog) != 275
        or sum(row["animation_frame_count"] == 0 for row in submotion_catalog) != 43
    ):
        raise ValueError("unexpected Falcon animated/empty submotion coverage")
    if (
        animation_node_count != 17271
        or animation_track_count != 38560
        or animation_key_count != 308057
    ):
        raise ValueError(
            "unexpected complete Falcon animation-track coverage: "
            f"nodes={animation_node_count} tracks={animation_track_count} "
            f"keys={animation_key_count}"
        )
    if (
        sum(row["motion_count"] != 0 for row in submotion_catalog) != 65
        or len(motion_x_f32) != 2536
        or len(motion_y_f32) != 2536
    ):
        raise ValueError(
            "unexpected complete Falcon translation coverage: "
            f"submotions={sum(row['motion_count'] != 0 for row in submotion_catalog)} "
            f"samples={len(motion_x_f32)}"
        )
    animation_tracks_sha256 = animation_tracks_digest.hexdigest()
    submotion_catalog_digest = hashlib.sha256(
        b"".join(
            struct.pack(
                ">4H2I",
                row["animation_frame_count"],
                row["gameplay_frame_count"],
                row["event_count"],
                row["event_offset"],
                row["animation_flags"],
                row["animation_size"],
            )
            for row in submotion_catalog
        )
    ).hexdigest()
    if len(script_events) != 2056 or len(script_bytes) != 16516:
        raise ValueError(
            "unexpected complete Falcon action-script coverage: "
            f"events={len(script_events)} bytes={len(script_bytes)}"
        )
    action_script_digest = hashlib.sha256(
        b"".join(
            struct.pack(
                ">HBB",
                event["byte_offset"],
                event["byte_count"],
                event["command_id"],
            )
            for event in script_events
        )
        + script_bytes
    ).hexdigest()
    common_attribute_bits, special_attributes = source_attributes(source_dat)
    collision_attributes = source_collision_attributes(source_dat)
    ledge_attributes = {
        "snap_x_f32": binary32(collision_attributes["ledge_snap_x"] * MELEE_X_TO_SIM),
        "snap_y_f32": binary32(collision_attributes["ledge_snap_y"] * MELEE_Y_TO_SIM),
        "snap_height_f32": binary32(
            collision_attributes["ledge_snap_height"] * MELEE_Y_TO_SIM
        ),
    }

    special_air_n_assignments = command_variable_assignments(subactions, 302)
    try:
        specialn_launch_frame = special_air_n_assignments[(0, 1)]
        specialn_scale_begin_frame = special_air_n_assignments[(1, 1)]
        specialn_air_physics_begin_frame = special_air_n_assignments[(1, 2)]
    except KeyError as error:
        raise ValueError("incomplete SpecialAirN command-variable timeline") from error
    if not (
        specialn_launch_frame == specialn_scale_begin_frame
        and specialn_scale_begin_frame < specialn_air_physics_begin_frame
    ):
        raise ValueError("invalid SpecialAirN command-variable ordering")
    special_s_ground_assignments = command_variable_assignments(subactions, 303)
    special_s_air_assignments = command_variable_assignments(subactions, 305)
    try:
        specials_ground_search_begin = special_s_ground_assignments[(0, 1)]
        specials_ground_search_end = special_s_ground_assignments[(0, 0)] - 1
        specials_air_search_begin = special_s_air_assignments[(0, 1)]
        specials_air_search_end = special_s_air_assignments[(0, 0)] - 1
        specials_air_gravity_begin = special_s_air_assignments[(1, 1)]
    except KeyError as error:
        raise ValueError("incomplete SpecialS command-variable timeline") from error
    if not (
        specials_ground_search_begin <= specials_ground_search_end
        and specials_air_search_begin <= specials_air_search_end
        and specials_air_gravity_begin >= specials_air_search_begin
    ):
        raise ValueError("invalid SpecialS command-variable ordering")
    common_special_attributes = source_common_special_attributes(common_dat)
    air_dodge_attributes = source_air_dodge_attributes(common_dat)
    air_dodge_assignments = command_variable_assignments(subactions, 44)
    try:
        air_dodge_ordinary_physics_begin_frame = air_dodge_assignments[(0, 1)]
    except KeyError as error:
        raise ValueError("incomplete EscapeAir command-variable timeline") from error
    if air_dodge_ordinary_physics_begin_frame != 30:
        raise ValueError("unexpected EscapeAir ordinary-physics transition")
    special_hi_ground_assignments = command_variable_assignments(subactions, 307)
    special_hi_air_assignments = command_variable_assignments(subactions, 308)
    special_hi_throw_assignments = command_variable_assignments(subactions, 310)
    try:
        specialhi_ground_control_begin = special_hi_ground_assignments[(0, 1)]
        specialhi_air_control_begin = special_hi_air_assignments[(0, 1)]
        specialhi_throw_gravity_begin = special_hi_throw_assignments[(0, 1)]
    except KeyError as error:
        raise ValueError("incomplete SpecialHi command-variable timeline") from error
    if not (
        specialhi_ground_control_begin == specialhi_air_control_begin
        and specialhi_ground_control_begin > 0
        and specialhi_throw_gravity_begin > 0
    ):
        raise ValueError("invalid SpecialHi command-variable ordering")
    special_lw_ground_assignments = command_variable_assignments(subactions, 311)
    special_lw_end_ground_assignments = command_variable_assignments(subactions, 312)
    special_lw_air_assignments = command_variable_assignments(subactions, 313)
    special_lw_landing_assignments = command_variable_assignments(subactions, 314)
    special_lw_end_air_from_ground_assignments = command_variable_assignments(
        subactions, 315
    )
    try:
        speciallw_ground_wall_rebound_begin = special_lw_ground_assignments[(0, 1)]
        speciallw_air_wall_rebound_begin = special_lw_air_assignments[(0, 1)]
        speciallw_ground_end_traction_begin = special_lw_end_ground_assignments[
            (2, 1)
        ]
        speciallw_ground_end_traction_end = (
            special_lw_end_ground_assignments[(2, 0)] - 1
        )
        speciallw_ground_end_edge_fall_begin = special_lw_end_ground_assignments[
            (1, 1)
        ]
        speciallw_landing_traction_begin = special_lw_landing_assignments[(2, 1)]
        speciallw_landing_traction_end = special_lw_landing_assignments[(2, 0)] - 1
        speciallw_ground_origin_air_physics_begin = (
            special_lw_end_air_from_ground_assignments[(0, 1)]
        )
        speciallw_ground_origin_edge_fall_begin = (
            special_lw_end_air_from_ground_assignments[(1, 1)]
        )
    except KeyError as error:
        raise ValueError("incomplete SpecialLw command-variable timeline") from error
    if not (
        speciallw_ground_wall_rebound_begin > 0
        and speciallw_air_wall_rebound_begin > 0
        and speciallw_ground_end_traction_begin <= speciallw_ground_end_traction_end
        and speciallw_landing_traction_begin <= speciallw_landing_traction_end
        and speciallw_ground_origin_air_physics_begin
        == speciallw_ground_origin_edge_fall_begin
    ):
        raise ValueError("invalid SpecialLw command-variable ordering")
    weight_independent_throws_word = common_attribute_bits[96]
    weight_independent_throws_mask = weight_independent_throws_word >> 24
    if (
        weight_independent_throws_word & 0x00FFFFFF
        or weight_independent_throws_mask & ~0x0F
    ):
        raise ValueError("invalid weight-independent throw mask")
    # These are action opcodes rather than command-variable writes. Decode the
    # exact script frames directly: opcode 29 enables the ordinary jab link,
    # opcode 30 enables rapid-jab selection, and the loop's five calls enter a
    # two-frame hit/decision subroutine.
    def action_opcode_frames(
        subaction_index: int,
        opcode: int,
    ) -> list[tuple[int, bytes]]:
        frame = 0
        result: list[tuple[int, bytes]] = []
        for event in subactions[subaction_index].get("events", []):
            command_id = int(str(event["commandId"]), 16)
            fields = event.get("fields") or {}
            if command_id == 0x08:
                frame = int(fields["frame"])
            elif command_id == 0x04:
                frame += int(fields["frames"])
            if command_id == opcode << 2:
                result.append((frame, bytes.fromhex(str(event["bytes"]))))
        return result

    jab_1_combo = action_opcode_frames(46, 29)
    jab_2_combo = action_opcode_frames(47, 29)
    jab_3_rapid = action_opcode_frames(48, 30)
    rapid_calls = action_opcode_frames(50, 5)
    down_tilt_continuation = action_opcode_frames(59, 52)
    if (
        jab_1_combo
        != [
            (5, bytes.fromhex("74 00 00 01")),
            (9, bytes.fromhex("74 00 00 00")),
        ]
        or jab_2_combo
        != [
            (4, bytes.fromhex("74 00 00 01")),
            (8, bytes.fromhex("74 00 00 00")),
        ]
        or jab_3_rapid != [(10, bytes.fromhex("78 00 00 01"))]
        or [frame for frame, _ in rapid_calls] != [4, 12, 20, 28, 35]
        or down_tilt_continuation
        != [
            (0, bytes.fromhex("d0 00 00 04")),
            (29, bytes.fromhex("d0 00 00 03")),
        ]
    ):
        raise ValueError("unexpected Falcon jab action-script timeline")
    common_attributes = {
        "initial_walk_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 0) * MELEE_X_TO_SIM
        ),
        "walk_acceleration_f32": binary32(
            raw_f32(common_attribute_bits, 1) * MELEE_X_TO_SIM
        ),
        "walk_maximum_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 2) * MELEE_X_TO_SIM
        ),
        "slow_walk_animation_scaling_f32": binary32(
            raw_f32(common_attribute_bits, 3) * MELEE_X_TO_SIM
        ),
        "middle_walk_animation_scaling_f32": binary32(
            raw_f32(common_attribute_bits, 4) * MELEE_X_TO_SIM
        ),
        "fast_walk_animation_scaling_f32": binary32(
            raw_f32(common_attribute_bits, 5) * MELEE_X_TO_SIM
        ),
        "run_animation_scaling_f32": binary32(
            raw_f32(common_attribute_bits, 11) * MELEE_X_TO_SIM
        ),
        "friction_f32": binary32(raw_f32(common_attribute_bits, 6) * MELEE_X_TO_SIM),
        "dash_initial_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 7) * MELEE_X_TO_SIM
        ),
        "dash_run_acceleration_a_f32": binary32(
            raw_f32(common_attribute_bits, 8) * MELEE_X_TO_SIM
        ),
        "dash_run_acceleration_b_f32": binary32(
            raw_f32(common_attribute_bits, 9) * MELEE_X_TO_SIM
        ),
        "dash_run_terminal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 10) * MELEE_X_TO_SIM
        ),
        "ground_maximum_horizontal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 13) * MELEE_X_TO_SIM
        ),
        "jump_horizontal_initial_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 15) * MELEE_X_TO_SIM
        ),
        "jump_vertical_initial_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 16) * MELEE_Y_TO_SIM
        ),
        "ground_air_jump_momentum_multiplier_f32": binary32(
            raw_f32(common_attribute_bits, 17)
        ),
        "jump_horizontal_maximum_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 18) * MELEE_X_TO_SIM
        ),
        "shorthop_vertical_initial_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 19) * MELEE_Y_TO_SIM
        ),
        "air_jump_multiplier_f32": binary32(raw_f32(common_attribute_bits, 20)),
        "double_jump_momentum_f32": binary32(raw_f32(common_attribute_bits, 21)),
        "double_jump_vertical_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 16)
            * raw_f32(common_attribute_bits, 20)
            * MELEE_Y_TO_SIM
        ),
        "double_jump_horizontal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 21) * MELEE_X_TO_SIM
        ),
        "gravity_f32": binary32(raw_f32(common_attribute_bits, 23) * MELEE_Y_TO_SIM),
        "terminal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 24) * MELEE_Y_TO_SIM
        ),
        "air_mobility_a_f32": binary32(
            raw_f32(common_attribute_bits, 25) * MELEE_X_TO_SIM
        ),
        "air_mobility_b_f32": binary32(
            raw_f32(common_attribute_bits, 26) * MELEE_X_TO_SIM
        ),
        "max_aerial_horizontal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 27) * MELEE_X_TO_SIM
        ),
        "air_friction_f32": binary32(
            raw_f32(common_attribute_bits, 28) * MELEE_X_TO_SIM
        ),
        "fast_fall_terminal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 29) * MELEE_Y_TO_SIM
        ),
        "maximum_horizontal_air_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 30) * MELEE_X_TO_SIM
        ),
        "shield_break_initial_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 37) * MELEE_Y_TO_SIM
        ),
        "rebound_animation_length_f32": binary32(
            raw_f32(common_attribute_bits, 39)
        ),
        "ledge_jump_horizontal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 42) * MELEE_X_TO_SIM
        ),
        "ledge_jump_vertical_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 43) * MELEE_Y_TO_SIM
        ),
        "wall_jump_horizontal_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 65) * MELEE_X_TO_SIM
        ),
        "wall_jump_vertical_velocity_f32": binary32(
            raw_f32(common_attribute_bits, 66) * MELEE_Y_TO_SIM
        ),
        # ftCo_800C6408 multiplies the per-fighter trophy scale by the
        # code-authored 1.497345 rise distance. Standard-match model scale is
        # 1.0 in the qualified ruleset.
        "match_entry_rise_f32": binary32(
            raw_f32(common_attribute_bits, 68)
            * 1.497345
            * MELEE_Y_TO_SIM
        ),
        "jump_startup_ticks": round(raw_f32(common_attribute_bits, 14)),
        "number_of_jumps": common_attribute_bits[22],
        "turn_duration_ticks": round(raw_f32(common_attribute_bits, 33)),
        "weight": round(raw_f32(common_attribute_bits, 34)),
        "jab_2_input_window_ticks": round(raw_f32(common_attribute_bits, 31)),
        "jab_3_input_window_ticks": round(raw_f32(common_attribute_bits, 32)),
        "rapid_jab_input_count": common_attribute_bits[38],
        "jab_1_combo_enable_frame": jab_1_combo[-1][0],
        "jab_2_combo_enable_frame": jab_2_combo[-1][0],
        "jab_3_rapid_enable_frame": jab_3_rapid[0][0],
        "rapid_jab_first_decision_frame": rapid_calls[0][0] + 2,
        "rapid_jab_decision_interval": rapid_calls[1][0] - rapid_calls[0][0],
        "rapid_jab_last_decision_frame": rapid_calls[-1][0] + 2,
        "rapid_jab_loop_frame_count": submotion_catalog[50][
            "gameplay_frame_count"
        ],
        "down_tilt_repeat_enable_frame": down_tilt_continuation[-1][0],
        "weight_independent_throws_mask": weight_independent_throws_mask,
        "reserved": 0,
        "normal_landing_lag_ticks": round(raw_f32(common_attribute_bits, 57)),
        "neutral_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 58)),
        "forward_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 59)),
        "back_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 60)),
        "up_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 61)),
        "down_aerial_landing_lag_ticks": round(raw_f32(common_attribute_bits, 62)),
    }

    complete_source_digest = hashlib.sha256(
        bytes.fromhex(EXPECTED_CANONICAL_SHA256)
        + bytes.fromhex(SOURCE_DAT_SHA256)
        + bytes.fromhex(SOURCE_ANIMATION_DAT_SHA256)
        + bytes.fromhex(SOURCE_COMMON_DAT_SHA256)
        + bytes.fromhex(BOUNCE_ECB_PROFILE_SHA256)
        + bytes.fromhex(BOUNCE_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(AERIAL_ATTACK_ECB_PROFILE_SHA256)
        + bytes.fromhex(AERIAL_ATTACK_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(SHIELD_BREAK_ECB_PROFILE_SHA256)
        + bytes.fromhex(SHIELD_BREAK_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(GUARD_ECB_PROFILE_SHA256)
        + bytes.fromhex(GUARD_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(DOWN_BOUND_ECB_PROFILE_SHA256)
        + bytes.fromhex(DOWN_BOUND_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(GETUP_ECB_PROFILE_SHA256)
        + bytes.fromhex(GETUP_ECB_SEMANTIC_SHA256)
        + bytes.fromhex(GROUND_LOOP_ECB_PROFILE_SHA256)
        + bytes.fromhex(GROUND_LOOP_ECB_SEMANTIC_SHA256)
        + b"".join(value.to_bytes(4, "big") for value in common_attribute_bits)
        + json.dumps(
            {
                "common_special": common_special_attributes,
                "air_dodge": {
                    **air_dodge_attributes,
                    "ordinary_physics_begin_frame": (
                        air_dodge_ordinary_physics_begin_frame
                    ),
                },
                "falcon_dive_victim_release_hitstun_ticks": (
                    SPECIALHI_CAPTURE_VICTIM_RELEASE_HITSTUN_TICKS
                ),
                "fighter_special": special_attributes,
                "fighter_collision": collision_attributes,
                "ledge_attacks": ledge_attacks,
                "down_bound_floor_contact_capture_sha256": (
                    DOWN_BOUND_FLOOR_CONTACT_CAPTURE_SHA256
                ),
                "down_bound_floor_contact_masks": {
                    "back": DOWN_BOUND_BACK_FLOOR_CONTACT_MASK,
                    "stomach": DOWN_BOUND_STOMACH_FLOOR_CONTACT_MASK,
                },
                "damage_fly_ecb_capture_sha256": (
                    DAMAGE_FLY_ECB_CAPTURE_SHA256
                ),
                "damage_fly_ecb_full_semantic_sha256": (
                    DAMAGE_FLY_ECB_FULL_SEMANTIC_SHA256
                ),
                "falcon_damage_fly_collision_pose_melee": {
                    "bottom_y_from_origin": DAMAGE_FLY_ECB_BOTTOM_Y_MELEE,
                    "top_y_from_origin": DAMAGE_FLY_ECB_TOP_Y_MELEE,
                    "side_x_from_origin": DAMAGE_FLY_ECB_SIDE_X_MELEE,
                    "side_y_from_origin": DAMAGE_FLY_ECB_SIDE_Y_MELEE,
                },
                "falcon_dive_grounded_throw_reposition_melee": {
                    "x": SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE,
                    "y": SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE,
                },
                "falcon_falling_collision_pose_melee": {
                    "bottom_y_from_origin": FALLING_ECB_BOTTOM_Y_MELEE,
                },
                "falcon_platform_drop_ecb_capture_sha256": (
                    PLATFORM_DROP_ECB_CAPTURE_SHA256
                ),
                "falcon_platform_drop_ecb_semantic_sha256": (
                    PLATFORM_DROP_ECB_SEMANTIC_SHA256
                ),
                "falcon_platform_drop_collision_pose_melee": {
                    "bottom_y_from_origin": PLATFORM_DROP_ECB_BOTTOM_Y_MELEE,
                },
                "falcon_airborne_ecb_capture_sha256": (
                    AIRBORNE_ECB_CAPTURE_SHA256
                ),
                "falcon_airborne_ecb_semantic_sha256": (
                    AIRBORNE_ECB_SEMANTIC_SHA256
                ),
                "falcon_airborne_collision_pose_f32": {
                    track_id: tuple(
                        tuple(
                            tuple(frame["ecb_f32"][point])
                            for point in ECB_POINTS
                        )
                        for frame in track["frames"]
                    )
                    for track_id, track in airborne_tracks.items()
                },
                "falcon_aerial_attack_ecb_capture_sha256": (
                    AERIAL_ATTACK_ECB_CAPTURE_SHA256
                ),
                "falcon_aerial_attack_ecb_semantic_sha256": (
                    AERIAL_ATTACK_ECB_SEMANTIC_SHA256
                ),
                "falcon_aerial_attack_bottom_y_f32": {
                    track_id: tuple(
                        float(frame["ecb_f32"]["bottom"][1])
                        for frame in track["frames"]
                    )
                    for track_id, track in aerial_attack_tracks.items()
                },
                "falcon_shield_break_ecb_capture_sha256": (
                    SHIELD_BREAK_ECB_CAPTURE_SHA256
                ),
                "falcon_shield_break_ecb_semantic_sha256": (
                    SHIELD_BREAK_ECB_SEMANTIC_SHA256
                ),
                "falcon_shield_break_fly_collision_pose_f32": tuple(
                    tuple(
                        tuple(frame["ecb_f32"][point])
                        for point in ECB_POINTS
                    )
                    for frame in shield_break_fly_frames
                ),
                "falcon_guard_ecb_capture_sha256": (
                    GUARD_ECB_CAPTURE_SHA256
                ),
                "falcon_guard_ecb_semantic_sha256": (
                    GUARD_ECB_SEMANTIC_SHA256
                ),
                "falcon_guard_collision_pose_f32": {
                    track_id: tuple(
                        tuple(
                            tuple(frame["ecb_f32"][point])
                            for point in ECB_POINTS
                        )
                        for frame in track["frames"]
                    )
                    for track_id, track in guard_tracks.items()
                },
                "falcon_down_bound_ecb_capture_sha256": (
                    DOWN_BOUND_ECB_CAPTURE_SHA256
                ),
                "falcon_down_bound_ecb_semantic_sha256": (
                    DOWN_BOUND_ECB_SEMANTIC_SHA256
                ),
                "falcon_down_bound_collision_pose_f32": {
                    "back": tuple(
                        tuple(
                            tuple(frame["ecb_f32"][point])
                            for point in ECB_POINTS
                        )
                        for frame in down_bound_back_frames
                    ),
                    "stomach": tuple(
                        tuple(
                            tuple(frame["ecb_f32"][point])
                            for point in ECB_POINTS
                        )
                        for frame in down_bound_stomach_frames
                    ),
                },
                "falcon_getup_ecb_capture_sha256": (
                    GETUP_ECB_CAPTURE_SHA256
                ),
                "falcon_getup_ecb_semantic_sha256": (
                    GETUP_ECB_SEMANTIC_SHA256
                ),
                "falcon_getup_collision_pose_f32": {
                    track_id: tuple(
                        tuple(
                            tuple(frame["ecb_f32"][point])
                            for point in ECB_POINTS
                        )
                        for frame in track["frames"]
                    )
                    for track_id, track in getup_tracks.items()
                },
                "falcon_ground_loop_ecb_capture_sha256": (
                    GROUND_LOOP_ECB_CAPTURE_SHA256
                ),
                "falcon_ground_loop_ecb_semantic_sha256": (
                    GROUND_LOOP_ECB_SEMANTIC_SHA256
                ),
                "falcon_crouch_wait_collision_pose_f32": tuple(
                    tuple(
                        tuple(frame["ecb_f32"][point])
                        for point in ECB_POINTS
                    )
                    for frame in crouch_wait_frames
                ),
                "falcon_air_dodge_collision_pose_melee": {
                    "bottom_y_from_origin": AIR_DODGE_ECB_BOTTOM_Y_MELEE,
                },
                "falcon_kick_ground_end_entry_velocity_scale": (
                    FALCON_KICK_GROUND_END_ENTRY_VELOCITY_SCALE
                ),
            },
            sort_keys=True,
            separators=(",", ":"),
        ).encode("ascii")
    ).hexdigest()

    if tuple(data) != MOVE_KEYS:
        raise ValueError("unexpected move order or incomplete Falcon table")

    for key in MOVE_KEYS:
        move = data[key]
        if move is None:
            if key not in EXPECTED_ABSENT_MOVE_KEYS:
                raise ValueError(f"{key}: incomplete Falcon frame-data row")
            moves.append({"present": 0})
            continue
        if key in EXPECTED_ABSENT_MOVE_KEYS:
            raise ValueError(f"{key}: unexpected Falcon frame-data row")

        phase_offset = len(phases)
        effect_offset = len(effects)
        move_effects = move.get("hitboxes", [])
        if len(move_effects) > 16:
            raise ValueError(f"{key}: too many hit effects")
        effects.extend(move_effects)
        for phase in move.get("hitFrames", []):
            mask = 0
            for effect_index in phase["hitboxes"]:
                if not 0 <= int(effect_index) < len(move_effects):
                    raise ValueError(f"{key}: invalid hit-effect index")
                mask |= 1 << int(effect_index)
            phases.append((int(phase["start"]), int(phase["end"]), mask))

        throw_index = 0xFFFF
        if "throw" in move:
            throw_index = len(throws)
            throw = throw_absolute_effect(
                dat_data,
                int(move["subactionIndex"]),
                str(move["subactionName"]),
            )
            throw["releaseFrame"] = (
                throw_release_frame(
                    dat_data,
                    int(move["subactionIndex"]),
                    str(move["subactionName"]),
                )
                if key in {"fthrow", "bthrow", "uthrow", "dthrow"}
                else 0
            )
            throws.append(throw)
        subaction_index = int(move["subactionIndex"])
        action_flags = struct.unpack_from(
            ">I",
            source_dat_block,
            subactions_offset + subaction_index * 0x18 + 0x10,
        )[0]
        motion_offset = submotion_catalog[subaction_index]["motion_offset"]
        motion_count = submotion_catalog[subaction_index]["motion_count"]
        if bool(action_flags & ANIMATION_TRANSLATION_FLAG) != bool(motion_count):
            raise ValueError(f"{key}: inconsistent translation span")
        moves.append(
            {
                "present": 1,
                "subaction": int(move["subactionIndex"]),
                "total": int(move["totalFrames"]),
                "iasa": u16(move.get("iasa")),
                "charge": u16(move.get("chargeFrame")),
                "autocancel_before": u16(move.get("autoCancelBefore")),
                "autocancel_after": u16(move.get("autoCancelAfter")),
                "landing": u16(move.get("landingLag")),
                "l_cancelled": u16(move.get("lcancelledLandingLag")),
                "phase_offset": phase_offset,
                "effect_offset": effect_offset,
                "throw_index": throw_index,
                "phase_count": len(move.get("hitFrames", [])),
                "effect_count": len(move_effects),
                "animation_flags": action_flags,
                "motion_offset": motion_offset,
                "motion_count": motion_count,
            }
        )

    lines = [
        "/* Generated by tools/import_ssbm_falcon_frame_data.py; do not edit. */",
        f"/* canonical source SHA-256: {EXPECTED_CANONICAL_SHA256} */",
        f"/* extractor revision: {EXTRACTOR_REVISION} */",
        f"/* DAT reader revision: {DAT_READER_REVISION} */",
        f"/* HSD animation reader revision: {HSD_READER_REVISION} */",
        f"/* PlCa.dat SHA-256: {SOURCE_DAT_SHA256} */",
        f"/* PlCaAJ.dat SHA-256: {SOURCE_ANIMATION_DAT_SHA256} */",
        f"/* PlCa.dat JSON SHA-256: {SOURCE_DAT_JSON_SHA256} */",
        f"/* PlCo.dat SHA-256: {SOURCE_COMMON_DAT_SHA256} */",
        f"/* bounce ECB profile SHA-256: {BOUNCE_ECB_PROFILE_SHA256} */",
        f"/* bounce ECB semantic SHA-256: {BOUNCE_ECB_SEMANTIC_SHA256} */",
        f"/* shield-break ECB profile SHA-256: {SHIELD_BREAK_ECB_PROFILE_SHA256} */",
        f"/* shield-break ECB semantic SHA-256: {SHIELD_BREAK_ECB_SEMANTIC_SHA256} */",
        f"/* guard ECB profile SHA-256: {GUARD_ECB_PROFILE_SHA256} */",
        f"/* guard ECB semantic SHA-256: {GUARD_ECB_SEMANTIC_SHA256} */",
        f"/* DownBound ECB profile SHA-256: {DOWN_BOUND_ECB_PROFILE_SHA256} */",
        f"/* DownBound ECB semantic SHA-256: {DOWN_BOUND_ECB_SEMANTIC_SHA256} */",
        f"/* getup ECB profile SHA-256: {GETUP_ECB_PROFILE_SHA256} */",
        f"/* getup ECB semantic SHA-256: {GETUP_ECB_SEMANTIC_SHA256} */",
        f"/* ground-loop ECB profile SHA-256: {GROUND_LOOP_ECB_PROFILE_SHA256} */",
        f"/* ground-loop ECB semantic SHA-256: {GROUND_LOOP_ECB_SEMANTIC_SHA256} */",
        f"/* complete Falcon source SHA-256: {complete_source_digest} */",
        f"/* complete 318-submotion catalog SHA-256: {submotion_catalog_digest} */",
        f"/* complete action-script SHA-256: {action_script_digest} */",
        f"/* complete decoded animation-track SHA-256: {animation_tracks_sha256} */",
        "",
        "static const uint8_t falcon_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{EXPECTED_CANONICAL_SHA256[index:index + 2]})"
            for index in range(0, len(EXPECTED_CANONICAL_SHA256), 2)
        ),
        "};",
        "",
        "static const uint8_t falcon_complete_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{complete_source_digest[index:index + 2]})"
            for index in range(0, len(complete_source_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t falcon_submotion_catalog_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{submotion_catalog_digest[index:index + 2]})"
            for index in range(0, len(submotion_catalog_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t falcon_action_script_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{action_script_digest[index:index + 2]})"
            for index in range(0, len(action_script_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t falcon_animation_tracks_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{animation_tracks_sha256[index:index + 2]})"
            for index in range(0, len(animation_tracks_sha256), 2)
        ),
        "};",
        "",
        "static const falcon_animation_decode_summary",
        "falcon_animation_decode_summary_data = {",
        f"    UINT32_C({animation_node_count}),",
        f"    UINT32_C({animation_track_count}),",
        f"    UINT32_C({animation_key_count}),",
        "};",
        "",
        "static const falcon_smash_charge_attributes",
        "falcon_smash_charge_attributes_source = {",
        f"    UINT16_C({smash_charge_ticks}),",
        f"    UINT16_C({smash_charge_damage_multiplier_q8}),",
        "};",
        "",
        "static const falcon_submotion_data",
        "falcon_submotions[PF_M4_FALCON_SUBMOTION_COUNT] = {",
    ]
    lines.extend(
        "    { "
        f"UINT16_C({row['animation_frame_count']}), "
        f"UINT16_C({row['gameplay_frame_count']}), "
        f"UINT16_C({row['event_count']}), "
        f"UINT16_C({row['event_offset']}), "
        f"UINT16_C({row['motion_offset']}), "
        f"UINT16_C({row['motion_count']}), "
        f"UINT32_C(0x{row['animation_flags']:08x}), "
        f"UINT32_C({row['animation_size']}) }},"
        for row in submotion_catalog
    )
    lines.extend([
        "};",
        "",
        "static const falcon_body_collision_timing",
        "falcon_body_collision_timings[PF_M4_FALCON_SUBMOTION_COUNT] = {",
    ])
    lines.extend(
        "    { "
        f"UINT16_C({state_two_frame}), UINT16_C({state_zero_frame}) }},"
        for state_two_frame, state_zero_frame in body_collision_timings
    )
    lines.extend([
        "};",
        "",
        "static const falcon_script_event",
        "falcon_script_events[PF_M4_FALCON_SCRIPT_EVENT_COUNT] = {",
    ])
    lines.extend(
        "    { "
        f"UINT16_C({event['byte_offset']}), "
        f"UINT8_C({event['byte_count']}), "
        f"UINT8_C(0x{event['command_id']:02x}) }},"
        for event in script_events
    )
    lines.extend([
        "};",
        "",
        "static const uint8_t",
        "falcon_script_bytes[PF_M4_FALCON_SCRIPT_BYTE_COUNT] = {",
    ])
    lines.extend(
        "    "
        + ", ".join(f"UINT8_C(0x{value:02x})" for value in script_bytes[index:index + 12])
        + ","
        for index in range(0, len(script_bytes), 12)
    )
    lines.extend([
        "};",
        "",
        "static const uint32_t",
        "falcon_common_attribute_bits[PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT] = {",
    ])
    lines.extend(
        "    "
        + ", ".join(
            f"UINT32_C(0x{value:08x})"
            for value in common_attribute_bits[index : index + 8]
        )
        + ","
        for index in range(0, len(common_attribute_bits), 8)
    )
    lines.extend(
        (
            "};",
            "",
            "static const falcon_common_attributes",
            "falcon_common_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT16_C({value}),"
            if name.endswith("_ticks")
            or name.endswith("_frame")
            or name
            in {
                "number_of_jumps",
                "weight",
                "rapid_jab_input_count",
                "rapid_jab_loop_frame_count",
                "rapid_jab_decision_interval",
            }
            else f"    .{name} = UINT8_C({value}),"
            if name in {"weight_independent_throws_mask", "reserved"}
            else f"    .{name} = {c_f32(value)},"
        )
        for name, value in common_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const falcon_ledge_attributes",
            "falcon_ledge_attribute_data = {",
        )
    )
    lines.extend(
        f"    .{name} = {c_f32(value)}," for name, value in ledge_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            f"/* qualified ledge-root capture SHA-256: {LEDGE_ROOT_CAPTURE_SHA256} */",
            "static const falcon_ledge_root_positions",
            "falcon_ledge_root_position_data = {",
            "    .catch_frame_one_x_f32 = "
            f"{c_f32(LEDGE_CATCH_FRAME_ONE_ROOT_MELEE[0] * MELEE_X_TO_SIM)},",
            "    .catch_frame_one_y_f32 = "
            f"{c_f32(-LEDGE_CATCH_FRAME_ONE_ROOT_MELEE[1] * MELEE_Y_TO_SIM)},",
            "    .wait_frame_one_x_f32 = "
            f"{c_f32(LEDGE_WAIT_FRAME_ONE_ROOT_MELEE[0] * MELEE_X_TO_SIM)},",
            "    .wait_frame_one_y_f32 = "
            f"{c_f32(-LEDGE_WAIT_FRAME_ONE_ROOT_MELEE[1] * MELEE_Y_TO_SIM)},",
            "    .option_frame_one_x_f32 = {",
            "        "
            + ", ".join(
                c_f32(value[0] * MELEE_X_TO_SIM)
                for value in LEDGE_OPTION_FRAME_ONE_ROOT_MELEE
            )
            + ",",
            "    },",
            "    .option_frame_one_y_f32 = {",
            "        "
            + ", ".join(
                c_f32(-value[1] * MELEE_Y_TO_SIM)
                for value in LEDGE_OPTION_FRAME_ONE_ROOT_MELEE
            )
            + ",",
            "    },",
            "    .option_ground_frame = {",
            "        "
            + ", ".join(
                f"UINT16_C({value})" for value in LEDGE_OPTION_GROUND_FRAME
            )
            + ",",
            "    },",
            "};",
            "",
            "static const falcon_ledge_attack_reference",
            "falcon_ledge_attack_references[2] = {",
            *(
                "    { "
                f"UINT16_C({attack['total_frames']}), "
                f"UINT16_C({attack['first_frame']}), "
                f"UINT16_C({attack['last_frame']}), UINT16_C(0), "
                f"{c_hit_effect(attack['effect'])} }},"
                for attack in ledge_attacks
            ),
            "};",
            "",
            "/* qualified Hyrule CliffJump1 trace SHA-256: "
            f"{LEDGE_JUMP1_HYRULE_SOURCE_TRACE_SHA256} */",
            "static const float",
            "falcon_hyrule_ledge_jump1_quick_from_wait_f32"
            "[PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT][2] = {",
            *(
                f"    {{ {c_f32(x)}, {c_f32(y)} }},"
                for x, y in LEDGE_JUMP1_HYRULE_QUICK_FROM_WAIT_F32
            ),
            "};",
            "static const float",
            "falcon_hyrule_ledge_jump1_slow_from_wait_f32"
            "[PF_M4_FALCON_LEDGE_JUMP1_SLOW_FRAME_COUNT][2] = {",
            *(
                f"    {{ {c_f32(x)}, {c_f32(y)} }},"
                for x, y in LEDGE_JUMP1_HYRULE_SLOW_FROM_WAIT_F32
            ),
            "};",
            "static const float",
            "falcon_hyrule_ledge_jump2_frame_one_from_wait_f32[2][2] = {",
            *(
                f"    {{ {c_f32(x)}, {c_f32(y)} }},"
                for x, y in LEDGE_JUMP2_HYRULE_FRAME_ONE_FROM_WAIT_F32
            ),
            "};",
            "",
            "static const falcon_common_special_attributes",
            "falcon_common_special_attribute_data = {",
        )
    )
    lines.extend(
        f"    .{name} = {c_f32(value)},"
        for name, value in common_special_attributes.items()
        if name != "stale_move_slot_reduction_f32"
    )
    lines.extend(
        (
            "};",
            "",
            "static const falcon_air_dodge_attributes",
            "falcon_air_dodge_attribute_data = {",
            "    .initial_velocity_x_f32 = "
            f"{c_f32(air_dodge_attributes['initial_velocity_x_f32'])},",
            "    .initial_velocity_y_f32 = "
            f"{c_f32(air_dodge_attributes['initial_velocity_y_f32'])},",
            "    .decay_f32 = "
            f"{c_f32(air_dodge_attributes['decay_f32'])},",
            "    .dead_zone = "
            f"UINT16_C({air_dodge_attributes['dead_zone']}),",
            "    .item_throw_window_ticks = "
            f"UINT16_C({air_dodge_attributes['item_throw_window_ticks']}),",
            "    .ordinary_physics_begin_frame = "
            f"UINT16_C({air_dodge_ordinary_physics_begin_frame}),",
            "    .reserved = UINT16_C(0),",
            "};",
            "",
            "static const melee_stale_move_data",
            "melee_stale_move_data_source = {",
            "    .slot_reduction_f32 = {",
            "        "
            + ", ".join(
                c_f32(value)
                for value in common_special_attributes["stale_move_slot_reduction_f32"]
            )
            + ",",
            "    },",
            "};",
            "",
            "static const falcon_special_attributes",
            "falcon_special_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT32_C({value}),"
            if name in SPECIAL_UNSIGNED_FIELDS
            else f"    .{name} = {c_f32(value)},"
            if name.endswith("_f32")
            else f"    .{name} = INT32_C({value}),"
        )
        for name, value in special_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const falcon_neutral_special_timing",
            "falcon_neutral_special_timing_data = {",
            f"    .launch_frame = UINT16_C({specialn_launch_frame}),",
            "    .velocity_scale_begin_frame = "
            f"UINT16_C({specialn_scale_begin_frame}),",
            "    .velocity_scale_end_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame - 1}),",
            "    .ordinary_air_physics_begin_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame}),",
            "};",
            "",
            "static const falcon_side_special_timing",
            "falcon_side_special_timing_data = {",
            "    .ground_search_begin_frame = "
            f"UINT16_C({specials_ground_search_begin}),",
            "    .ground_search_end_frame = "
            f"UINT16_C({specials_ground_search_end}),",
            "    .air_search_begin_frame = " f"UINT16_C({specials_air_search_begin}),",
            "    .air_search_end_frame = " f"UINT16_C({specials_air_search_end}),",
            "    .air_gravity_begin_frame = "
            f"UINT16_C({specials_air_gravity_begin}),",
            "};",
            "",
            "static const falcon_up_special_timing",
            "falcon_up_special_timing_data = {",
            "    .air_control_begin_frame = "
            f"UINT16_C({specialhi_ground_control_begin}),",
            "    .throw_gravity_begin_frame = "
            f"UINT16_C({specialhi_throw_gravity_begin}),",
            "    .victim_release_hitstun_ticks = "
            f"UINT16_C({SPECIALHI_CAPTURE_VICTIM_RELEASE_HITSTUN_TICKS}),",
            "    .reserved = UINT16_C(0),",
            "    .grounded_throw_reposition_x_f32 = "
            f"{c_f32(SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE * MELEE_X_TO_SIM)},",
            "    .grounded_throw_reposition_y_f32 = "
            f"{c_f32(-SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE * MELEE_Y_TO_SIM)},",
            "};",
            "",
            "static const falcon_down_special_timing",
            "falcon_down_special_timing_data = {",
            "    .ground_wall_rebound_begin_frame = "
            f"UINT16_C({speciallw_ground_wall_rebound_begin}),",
            "    .air_wall_rebound_begin_frame = "
            f"UINT16_C({speciallw_air_wall_rebound_begin}),",
            "    .ground_end_traction_begin_frame = "
            f"UINT16_C({speciallw_ground_end_traction_begin}),",
            "    .ground_end_traction_end_frame = "
            f"UINT16_C({speciallw_ground_end_traction_end}),",
            "    .ground_end_edge_fall_begin_frame = "
            f"UINT16_C({speciallw_ground_end_edge_fall_begin}),",
            "    .landing_traction_begin_frame = "
            f"UINT16_C({speciallw_landing_traction_begin}),",
            "    .landing_traction_end_frame = "
            f"UINT16_C({speciallw_landing_traction_end}),",
            "    .ground_origin_air_physics_begin_frame = "
            f"UINT16_C({speciallw_ground_origin_air_physics_begin}),",
            "    .ground_origin_edge_fall_begin_frame = "
            f"UINT16_C({speciallw_ground_origin_edge_fall_begin}),",
            "    .reserved = UINT16_C(0),",
            "    .ground_end_entry_velocity_scale_f32 = "
            f"{c_f32(FALCON_KICK_GROUND_END_ENTRY_VELOCITY_SCALE)},",
            "};",
            "",
            "static const falcon_collision_pose",
            "falcon_collision_pose_data = {",
            "    .falling_bottom_y_from_origin_f32 = "
            f"{c_f32(FALLING_ECB_BOTTOM_Y_MELEE * MELEE_Y_TO_SIM)},",
            "    .crouch_wait = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in crouch_wait_frames
            ),
            "    },",
            "    .down_bound_floor_contact_mask = {",
            "        "
            f"UINT32_C({DOWN_BOUND_BACK_FLOOR_CONTACT_MASK}), "
            f"UINT32_C({DOWN_BOUND_STOMACH_FLOOR_CONTACT_MASK}),",
            "    },",
            "    .down_bound = {",
            *render_ecb_pose_track_table(
                (down_bound_back_frames, down_bound_stomach_frames),
                "        ",
            ),
            "    },",
            "    .down_wait = {",
            *render_ecb_pose_track_table(down_wait_frames, "        "),
            "    },",
            "    .getup_neutral = {",
            *render_ecb_pose_track_table(getup_neutral_frames, "        "),
            "    },",
            "    .getup_attack = {",
            *render_ecb_pose_track_table(getup_attack_frames, "        "),
            "    },",
            "    .getup_roll = {",
            *render_ecb_pose_track_matrix(getup_roll_frames, "        "),
            "    },",
            "    .damage_fly_bottom_y_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_Y_TO_SIM)
                for value in DAMAGE_FLY_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_top_y_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_Y_TO_SIM)
                for value in DAMAGE_FLY_ECB_TOP_Y_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_side_x_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_X_TO_SIM)
                for value in DAMAGE_FLY_ECB_SIDE_X_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_side_y_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_Y_TO_SIM)
                for value in DAMAGE_FLY_ECB_SIDE_Y_MELEE
            )
            + ",",
            "    },",
            "    .air_dodge_bottom_y_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_Y_TO_SIM)
                for value in AIR_DODGE_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .platform_drop_bottom_y_from_origin_f32 = {",
            "        "
            + ", ".join(
                c_f32(value * MELEE_Y_TO_SIM)
                for value in PLATFORM_DROP_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .airborne = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in airborne_frames
            ),
            "    },",
            "    .shield_break_fly = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in shield_break_fly_frames
            ),
            "    },",
            "    .shield_break_down_down = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in shield_break_down_down_frames
            ),
            "    },",
            "    .shield_break_stand_down = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in shield_break_stand_down_frames
            ),
            "    },",
            "    .shield_break_stun = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in shield_break_stun_frames
            ),
            "    },",
            "    .guard_on = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in guard_on_frames
            ),
            "    },",
            "    .guard = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in guard_frames
            ),
            "    },",
            "    .guard_off = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in guard_off_frames
            ),
            "    },",
            "    .ceiling_bounce = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in ceiling_bounce_frames
            ),
            "    },",
            "    .wall_bounce = {",
            *(
                f"        {render_ecb_pose_f32(frame)},"
                for frame in wall_bounce_frames
            ),
            "    },",
            "};",
            "",
            "static const reference_hit_phase falcon_hit_phases[] = {",
        )
    )
    lines.extend(
        f"    {{ UINT16_C({start}), UINT16_C({end}), UINT16_C({mask}), UINT16_C(0) }},"
        for start, end, mask in phases
    )
    lines.extend(
        (
            "};",
            "",
            "static const reference_hit_effect falcon_hit_effects[] = {",
        )
    )
    for effect in effects:
        lines.append(f"    {c_hit_effect(effect)},")
    lines.extend(
        ("};", "", "static const reference_throw falcon_throws[] = {")
    )
    for throw in throws:
        lines.append(
            "    { "
            f"UINT16_C({int(throw['angle'])}), "
            f"UINT16_C({int(throw['kbGrowth'])}), "
            f"UINT16_C({int(throw['weightDepKb'])}), "
            f"UINT16_C({int(throw['baseKb'])}), "
            f"UINT8_C({int(throw['damage'])}), "
            f"UINT8_C({int(throw['element'])}), "
            f"UINT16_C({int(throw['releaseFrame'])}), "
            "UINT16_C(0) },"
        )
    lines.extend(
        (
            "};",
            "",
            "static const struct reference_move falcon_moves[PF_M4_FALCON_MOVE_COUNT] = {",
        )
    )
    for move in moves:
        if move["present"] == 0:
            lines.append("    { 0 },")
            continue
        lines.append(
            "    { "
            f"UINT16_C({move['subaction']}), UINT16_C({move['total']}), "
            f"UINT16_C({move['iasa']}), UINT16_C({move['charge']}), "
            f"UINT16_C({move['autocancel_before']}), "
            f"UINT16_C({move['autocancel_after']}), "
            f"UINT16_C({move['landing']}), UINT16_C({move['l_cancelled']}), "
            f"UINT16_C({move['phase_offset']}), UINT16_C({move['effect_offset']}), "
            f"UINT16_C({move['throw_index']}), UINT8_C({move['phase_count']}), "
            f"UINT8_C({move['effect_count']}), UINT8_C(1), UINT8_C(0), "
            f"UINT32_C({move['animation_flags']}), "
            f"UINT16_C({move['motion_offset']}), "
            f"UINT16_C({move['motion_count']}) }},"
        )
    lines.extend(
        (
            "};",
            "",
            "static const float "
            "falcon_translation_x_f32[PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT] = {",
        )
    )
    lines.extend(
        "    "
        + ", ".join(c_f32(value) for value in motion_x_f32[index : index + 8])
        + ","
        for index in range(0, len(motion_x_f32), 8)
    )
    lines.extend(
        (
            "};",
            "",
            "static const float "
            "falcon_translation_y_f32[PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT] = {",
        )
    )
    lines.extend(
        "    "
        + ", ".join(c_f32(value) for value in motion_y_f32[index : index + 8])
        + ","
        for index in range(0, len(motion_y_f32), 8)
    )
    lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("dat_source", type=Path)
    parser.add_argument("source_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("common_dat", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.source.read_text(encoding="utf-8"))
    dat_bytes = args.dat_source.read_bytes()
    dat_digest = hashlib.sha256(dat_bytes).hexdigest()
    if dat_digest != SOURCE_DAT_JSON_SHA256:
        raise SystemExit(f"unexpected Falcon DAT JSON SHA-256: {dat_digest}")
    dat_data = json.loads(dat_bytes)
    source_dat = args.source_dat.read_bytes()
    source_dat_digest = hashlib.sha256(source_dat).hexdigest()
    if source_dat_digest != SOURCE_DAT_SHA256:
        raise SystemExit(f"unexpected PlCa.dat SHA-256: {source_dat_digest}")
    animation_dat = args.animation_dat.read_bytes()
    animation_dat_digest = hashlib.sha256(animation_dat).hexdigest()
    if animation_dat_digest != SOURCE_ANIMATION_DAT_SHA256:
        raise SystemExit("unexpected PlCaAJ.dat SHA-256: " f"{animation_dat_digest}")
    common_dat = args.common_dat.read_bytes()
    common_dat_digest = hashlib.sha256(common_dat).hexdigest()
    if common_dat_digest != SOURCE_COMMON_DAT_SHA256:
        raise SystemExit(f"unexpected PlCo.dat SHA-256: {common_dat_digest}")
    bounce_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data") / "ssbm_falcon_bounce_ecb.json",
        expected_profile_sha256=BOUNCE_ECB_PROFILE_SHA256,
        expected_capture_sha256=BOUNCE_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=BOUNCE_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("ceiling_bounce", "BOUNCE_CEILING", 0, 9),
            ("wall_bounce", "BOUNCE_WALL", 0, 51),
        ),
    )
    airborne_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data") / "ssbm_falcon_airborne_ecb.json",
        expected_profile_sha256=AIRBORNE_ECB_PROFILE_SHA256,
        expected_capture_sha256=AIRBORNE_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=AIRBORNE_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("jump_forward", "JUMPING_FORWARD", 1, 35),
            ("jump_backward", "JUMPING_BACKWARD", 1, 50),
            ("jump_aerial_forward", "JUMPING_ARIAL_FORWARD", 1, 50),
            ("jump_aerial_backward", "JUMPING_ARIAL_BACKWARD", 1, 35),
            ("fall", "FALLING", 1, 8),
            ("fall_aerial", "FALLING_AERIAL", 1, 8),
        ),
    )
    aerial_attack_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_aerial_attack_ecb.json",
        expected_profile_sha256=AERIAL_ATTACK_ECB_PROFILE_SHA256,
        expected_capture_sha256=AERIAL_ATTACK_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=AERIAL_ATTACK_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("nair", "NAIR", 1, 44),
            ("fair", "FAIR", 1, 39),
            ("bair", "BAIR", 1, 35),
            ("uair", "UAIR", 1, 33),
            ("dair", "DAIR", 1, 44),
        ),
    )
    shield_break_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_shield_break_ecb.json",
        expected_profile_sha256=SHIELD_BREAK_ECB_PROFILE_SHA256,
        expected_capture_sha256=SHIELD_BREAK_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=SHIELD_BREAK_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("shield-break-fly", "SHIELD_BREAK_FLY", 1, 42),
            ("shield-break-down-down", "SHIELD_BREAK_DOWN_D", 1, 26),
            ("shield-break-stand-down", "SHIELD_BREAK_STAND_D", 1, 30),
            ("shield-break-stun", "SHIELD_BREAK_TEETER", 0, 100),
        ),
    )
    guard_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_guard_ecb.json",
        expected_profile_sha256=GUARD_ECB_PROFILE_SHA256,
        expected_capture_sha256=GUARD_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=GUARD_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("guard_on", "SHIELD_START", 0, 8),
            ("guard", "SHIELD", 0, 1),
            ("guard_off", "SHIELD_RELEASE", 0, 16),
        ),
    )
    down_bound_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_down_bound_ecb.json",
        expected_profile_sha256=DOWN_BOUND_ECB_PROFILE_SHA256,
        expected_capture_sha256=DOWN_BOUND_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=DOWN_BOUND_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("down_bound_stomach", "TECH_MISS_DOWN", 1, 26),
            ("down_bound_back", "TECH_MISS_UP", 1, 26),
        ),
    )
    getup_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_getup_ecb.json",
        expected_profile_sha256=GETUP_ECB_PROFILE_SHA256,
        expected_capture_sha256=GETUP_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=GETUP_ECB_SEMANTIC_SHA256,
        expected_tracks=(
            ("down_wait_stomach", "LYING_GROUND_DOWN", 0, 70),
            ("down_wait_back", "LYING_GROUND_UP", 0, 70),
            ("getup_neutral_stomach", "NEUTRAL_GETUP", 1, 30),
            ("getup_attack_stomach", "GETUP_ATTACK", 1, 49),
            ("getup_roll_forward_stomach", "GROUND_ROLL_FORWARD_DOWN", 1, 35),
            ("getup_roll_backward_stomach", "GROUND_ROLL_BACKWARD_DOWN", 1, 35),
            ("getup_neutral_back", "GROUND_GETUP", 1, 30),
            ("getup_attack_back", "GROUND_ATTACK_UP", 1, 49),
            ("getup_roll_forward_back", "GROUND_ROLL_FORWARD_UP", 1, 35),
            ("getup_roll_backward_back", "GROUND_ROLL_BACKWARD_UP", 1, 35),
        ),
    )
    ground_loop_ecb_profile = load_ecb_profile(
        Path(__file__).with_name("data")
        / "ssbm_falcon_ground_loop_ecb.json",
        expected_profile_sha256=GROUND_LOOP_ECB_PROFILE_SHA256,
        expected_capture_sha256=GROUND_LOOP_ECB_CAPTURE_SHA256,
        expected_semantic_sha256=GROUND_LOOP_ECB_SEMANTIC_SHA256,
        expected_tracks=(("crouch_wait", "CROUCHING", 0, 158),),
    )
    digest = canonical_sha256(data)
    if digest != EXPECTED_CANONICAL_SHA256:
        raise SystemExit(f"unexpected Falcon frame-data SHA-256: {digest}")
    output = generate(
        data,
        dat_data,
        source_dat,
        animation_dat,
        common_dat,
        bounce_ecb_profile,
        airborne_ecb_profile,
        aerial_attack_ecb_profile,
        shield_break_ecb_profile,
        guard_ecb_profile,
        down_bound_ecb_profile,
        getup_ecb_profile,
        ground_loop_ecb_profile,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-frame-data=pass "
        f"slots={len(MOVE_KEYS)} "
        f"subactions={sum(data[key] is not None for key in MOVE_KEYS)} "
        f"catalog={SUBMOTION_COUNT} animated=275 empty=43 "
        f"script_events=2056 script_bytes=16516 "
        f"animation_nodes=17271 animation_tracks=38560 "
        f"animation_keys=308057 "
        f"translation_submotions=65 translation_samples=2536 "
        f"source_sha256={digest} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
