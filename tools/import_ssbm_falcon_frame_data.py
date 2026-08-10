#!/usr/bin/env python3
"""Convert the pinned Falcon NTSC 1.02 frame-data dump to compact C tables."""

from __future__ import annotations

import argparse
import hashlib
import json
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
    pose_q16 as ecb_pose_q16,
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
SPECIALHI_LEDGE_ECB_CAPTURE_SHA256 = (
    "5a5b295d0fc7a8d1c06512dc704176a131a7c01a931a0a2b92f6d7ff8c3a8295"
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
    "407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba"
)
AIRBORNE_ECB_CAPTURE_SHA256 = (
    "4e6768e0862307eb32a14532fae8e2991e2900ea932b7af45850803c2ec8673f"
)
AIRBORNE_ECB_SEMANTIC_SHA256 = (
    "21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a"
)
AERIAL_ATTACK_ECB_PROFILE_SHA256 = (
    "209fa9712c2f12f81f9eededd15e08fcfaef20f87cffc3f0f00a4c6d42f50b04"
)
AERIAL_ATTACK_ECB_CAPTURE_SHA256 = (
    "9978972ba84a870ae5456c2403234d837c8b425f6dde4f3df83993a809e5534d"
)
AERIAL_ATTACK_ECB_SEMANTIC_SHA256 = (
    "55e686a07cf3d064618104051f0085ed2a398e9a1612847200b2cba51a665f10"
)
BOUNCE_ECB_PROFILE_SHA256 = (
    "d6ccb5701f0bada0d7de1874004281e8ca46fcc0070db94e529d84d3fc637608"
)
BOUNCE_ECB_CAPTURE_SHA256 = (
    "f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2"
)
BOUNCE_ECB_SEMANTIC_SHA256 = (
    "9d162fe7917f0c23894ad1fe54a1a665d5c8e446d5ca439180811d706b2431a5"
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
LEDGE_JUMP1_HYRULE_QUICK_FROM_WAIT_Q16 = (
    (-2241, -19023), (-4704, -39525), (-7286, -60950),
    (-9886, -82744), (-11067, -104351), (-11406, -125215),
    (-10904, -144782), (-11240, -155183), (-14591, -168594),
    (-14382, -199956), (-14753, -237028),
)
LEDGE_JUMP1_HYRULE_SLOW_FROM_WAIT_Q16 = (
    (-1276, -18300), (-2717, -37657), (-4235, -57819),
    (-5741, -78532), (-7148, -99543), (-8369, -120598),
    (-9315, -141444), (-9956, -163244), (-10345, -186483),
    (-10520, -209898), (-10518, -232223), (-10379, -252195),
    (-10140, -268548), (-9157, -274407), (-7621, -270851),
    (-6823, -268548), (-7130, -272307), (-8174, -277320),
)
LEDGE_JUMP2_HYRULE_FRAME_ONE_FROM_WAIT_Q16 = (
    (-20192, -275398),
    (-15013, -315690),
)

COMMON_ATTRIBUTE_COUNT = 97
SUBMOTION_COUNT = 318
SPECIAL_ATTRIBUTE_SIZE = 0x8C
STALE_MOVE_SLOT_COUNT = 9

# ftCaptain_DatAttrs at doldecomp/melee revision 9509dc0. Keep the raw words
# as the source of truth; the typed Q16 view below exists only so the runtime
# never has to reinterpret host floats or duplicate these values by hand.
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

ELEMENTS = {
    "empty": "PF_M4_REFERENCE_HIT_EMPTY",
    "normal": "PF_M4_REFERENCE_HIT_NORMAL",
    "fire": "PF_M4_REFERENCE_HIT_FIRE",
    "electric": "PF_M4_REFERENCE_HIT_ELECTRIC",
    "grab": "PF_M4_REFERENCE_HIT_GRAB",
}

ANIMATION_TRANSLATION_FLAG = 0x80000000
ANIMATION_TRANSLATION_NODE_MASK = 0x3F
MELEE_X_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0
MELEE_Y_TO_SIM_Q16 = 65536.0 * 11.0 / 62.0

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

# Pass is a 30-frame common submotion (index 244). Its animation clock is
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
# captured Hyrule routes produce the same 24-frame Q16 table and semantic
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

# FallSpecial is a distinct common animation from Falling. These eight live
# ECB-bottom samples are the complete pre-landing route observed in the pinned
# up-ground-miss capture (SHA-256
# 97672ddf0e5013beaad8ff4c31f54c6bae93551ca3a38755cc3d185bcd5b83c4),
# independently confirmed by the aerial-miss capture (SHA-256
# 9ecf456e6377f5b7d371ccb84c9f5bd7b3a1045724a7c223acb6cb9d4681fd21).
# Reusing Falling's bottom here moves Falcon Dive's landing by multiple frames.
FALL_SPECIAL_ECB_BOTTOM_Y_MELEE = (
    2.3061580657958984,
    2.11196231842041,
    2.46517276763916,
    2.718437671661377,
    2.784078598022461,
    2.7656466960906982,
    2.499094247817993,
    2.2265543937683105,
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

# SpecialAirS is collision-animated independently of the common airborne body.
# These are all displayed frames 0 through 44 from the pinned direct-memory
# capture SHA-256
# 86e0abff2d1de0483e25ef8db045da323a35331bf95fb7089b00283233b4fc8e.
# The separate natural-floor capture proves that frame 34's bottom delays
# LandingFallSpecial by one frame compared with the generic body extent.
RAPTOR_BOOST_HIT_AIR_ECB_BOTTOM_Y_MELEE = (
    2.21038818359375,
    1.148101806640625,
    1.119384765625,
    1.149749755859375,
    1.190185546875,
    1.200103759765625,
    1.35498046875,
    0.0,
    0.0,
    0.97576904296875,
    1.887542724609375,
    2.355865478515625,
    2.4384765625,
    2.51934814453125,
    2.494232177734375,
    2.37939453125,
    2.3909912109375,
    2.5333251953125,
    2.59649658203125,
    2.580230712890625,
    2.52886962890625,
    2.797637939453125,
    1.94158935546875,
    1.239166259765625,
    0.95928955078125,
    1.274505615234375,
    2.072601318359375,
    2.61260986328125,
    2.182159423828125,
    1.88592529296875,
    1.7708740234375,
    1.70269775390625,
    1.670013427734375,
    1.6629638671875,
    1.672943115234375,
    1.69244384765625,
    1.714813232421875,
    1.734375,
    1.746063232421875,
    1.745758056640625,
    1.730194091796875,
    1.697662353515625,
    1.83013916015625,
    2.544525146484375,
    2.3905029296875,
)

# Complete collision-step 1..64 horizontal ECB extent for SpecialAirHi,
# captured from fighter+0x794 in the pinned NTSC 1.02 ledge route above. Frame
# 64 is the catch step: CollData retains SpecialAirHi's pre-snap position while
# the post-collision ECB already reflects EdgeCatch. The
# decomp's mpColl_80044164 adds this animated extent to ftData_x44's authored
# ledge-snap X; using a generic body width changes Falcon Dive's catch frame.
SPECIALHI_ECB_RIGHT_X_MELEE = (
    4.6822509765625,
    3.2646751403808594,
    3.2517662048339844,
    8.790153503417969,
    9.282890319824219,
    9.3123779296875,
    9.328704833984375,
    9.3343505859375,
    9.332077026367188,
    9.322662353515625,
    9.302871704101562,
    9.272300720214844,
    9.225509643554688,
    4.481849670410156,
    2.7993927001953125,
    3.1050453186035156,
    3.1559104919433594,
    3.2133026123046875,
    3.277557373046875,
    3.3464431762695312,
    3.4177474975585938,
    3.49127197265625,
    3.576812744140625,
    3.8591537475585938,
    4.148586273193359,
    4.3865203857421875,
    4.665973663330078,
    4.6155853271484375,
    4.551082611083984,
    4.699317932128906,
    4.574001312255859,
    3.7549285888671875,
    3.1483802795410156,
    3.94476318359375,
    4.5456695556640625,
    11.011032104492188,
    10.624046325683594,
    4.975502014160156,
    3.980335235595703,
    3.3603744506835938,
    2.94268798828125,
    3.6852798461914062,
    3.100555419921875,
    2.7566299438476562,
    3.8938522338867188,
    4.600364685058594,
    6.003654479980469,
    5.736976623535156,
    5.63336181640625,
    6.48797607421875,
    3.9386558532714844,
    2.584308624267578,
    2.0,
    2.0,
    2.3788604736328125,
    2.775951385498047,
    2.3810043334960938,
    2.3954505920410156,
    2.3052520751953125,
    2.1954994201660156,
    2.63751220703125,
    2.8061485290527344,
    2.899059295654297,
    3.6350555419921875,
)

SPECIALHI_ECB_BOTTOM_Y_MELEE = (
    3.055513381958008,
    4.40119743347168,
    4.645145416259766,
    4.39903450012207,
    2.1715545654296875,
    2.0419673919677734,
    1.9570884704589844,
    1.9130744934082031,
    1.9060840606689453,
    1.9322776794433594,
    1.987813949584961,
    2.0688552856445312,
    2.1715545654296875,
    2.1715545654296875,
    2.1715545654296875,
    2.1715545654296875,
    2.1715545654296875,
    2.0919408798217773,
    2.034531593322754,
    1.926483154296875,
    1.8232789039611816,
    1.7804226875305176,
    1.8533048629760742,
    2.0940933227539062,
    2.4376063346862793,
    2.846829414367676,
    3.2853517532348633,
    3.712709426879883,
    4.111310005187988,
    4.468051910400391,
    4.8230133056640625,
    4.9176177978515625,
    5.207601547241211,
    5.917392730712891,
    6.684072494506836,
    7.168909072875977,
    7.242408752441406,
    7.64799690246582,
    8.447587966918945,
    8.544607162475586,
    9.129560470581055,
    11.209505081176758,
    13.178729057312012,
    12.47581958770752,
    12.429488182067871,
    12.864155769348145,
    13.55915355682373,
    14.357234954833984,
    13.16108226776123,
    11.59609603881836,
    9.9263916015625,
    7.727045059204102,
    6.414306640625,
    6.587861061096191,
    8.260906219482422,
    9.847517967224121,
    12.090156555175781,
    11.625774383544922,
    10.76664924621582,
    9.730381965637207,
    8.729700088500977,
    7.349581241607666,
    4.837783336639404,
    3.194936752319336,
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


def render_ecb_pose_q16(frame: dict[str, Any]) -> str:
    ecb = frame["ecb_q16"]
    values = [
        value
        for point in ECB_POINTS
        for value in ecb[point]
    ]
    return "{ " + ", ".join(f"INT32_C({value})" for value in values) + " }"


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
        profile.get("schema") != 1
        or profile.get("scope") != "ssbm-action-owned-ecb-pose-tracks"
        or profile.get("capture_sha256") != expected_capture_sha256
        or profile.get("semantic_sha256") != expected_semantic_sha256
        or profile.get("coordinate_conversion")
        != {
            "rounding": "nearest-python-round",
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
            q16_ecb = frame.get("ecb_q16")
            if (
                not isinstance(frame, dict)
                or frame.get("displayed_frame") != displayed_frame
                or not isinstance(source_ecb, dict)
                or not isinstance(q16_ecb, dict)
                or set(source_ecb) != set(ECB_POINTS)
                or set(q16_ecb) != set(ECB_POINTS)
                or ecb_pose_q16(source_ecb) != q16_ecb
                or any(
                    not isinstance(value, int)
                    or isinstance(value, bool)
                    or not -(1 << 31) <= value < (1 << 31)
                    for point in ECB_POINTS
                    for value in q16_ecb[point]
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


def animation_translation_q16(
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
            round(
                (positions_x[frame] - positions_x[frame - 1])
                * model_scaling
                * MELEE_X_TO_SIM_Q16
            )
            for frame in range(1, frame_count)
        ],
        [
            round(
                -(positions_y[frame] - positions_y[frame - 1])
                * model_scaling
                * MELEE_Y_TO_SIM_Q16
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


def q16(value: float) -> int:
    return round(value * 65536.0)


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
        special[f"{name}_q16"] = q16(value)
    for name, kind, offset in SPECIAL_TAIL_ATTRIBUTES:
        if kind == "f32":
            value = struct.unpack_from(">f", data, special_offset + offset)[0]
            special[f"{name}_q16"] = q16(value)
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
        "ecb_minimum_q16": q16(joints_and_scale[6]),
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
        "fast_ground_friction_multiplier_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x6C)[0]
        ),
        "air_drift_over_maximum_deceleration_q16": round(
            struct.unpack_from(">f", data, common_offset + 0x1FC)[0]
            * MELEE_X_TO_SIM_Q16
        ),
        "side_special_stick_threshold_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x218)[0]
        ),
        "side_special_turn_threshold_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x220)[0]
        ),
        "air_drift_dead_zone_q16": q16(
            struct.unpack_from(">f", data, common_offset + 0x258)[0]
        ),
        "stale_move_slot_reduction_q16": [
            q16(struct.unpack_from(">f", data, stale_move_offset + index * 4)[0])
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
        # velocity so the fixed-point runtime does not need a wider transient.
        "initial_velocity_x_q16": round(
            force * decay * MELEE_X_TO_SIM_Q16
        ),
        "initial_velocity_y_q16": round(
            force * decay * MELEE_Y_TO_SIM_Q16
        ),
        "decay_q16": q16(decay),
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
) -> str:
    phases: list[tuple[int, int, int]] = []
    effects: list[dict[str, Any]] = []
    throws: list[dict[str, Any]] = []
    moves: list[dict[str, int]] = []
    motion_x_q16: list[int] = []
    motion_y_q16: list[int] = []
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
    aerial_attack_frames = tuple(
        frame
        for track_id in ("nair", "fair", "bair", "uair", "dair")
        for frame in aerial_attack_tracks[track_id]["frames"]
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
        motion_offset = len(motion_x_q16)
        if animation_flags & ANIMATION_TRANSLATION_FLAG:
            if tree is None:
                raise ValueError(
                    f"submotion {submotion_index}: translation without animation"
                )
            submotion_motion_x_q16, submotion_motion_y_q16 = (
                animation_translation_q16(
                    tree,
                    animation_flags,
                    model_scaling,
                )
            )
            if (
                len(submotion_motion_x_q16) != animation_frame_count - 1
                or len(submotion_motion_y_q16) != animation_frame_count - 1
            ):
                raise ValueError(
                    f"submotion {submotion_index}: incomplete translation samples"
                )
            motion_x_q16.extend(submotion_motion_x_q16)
            motion_y_q16.extend(submotion_motion_y_q16)
        motion_count = len(motion_x_q16) - motion_offset
        if len(motion_x_q16) != len(motion_y_q16):
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
        or len(motion_x_q16) != 2536
        or len(motion_y_q16) != 2536
    ):
        raise ValueError(
            "unexpected complete Falcon translation coverage: "
            f"submotions={sum(row['motion_count'] != 0 for row in submotion_catalog)} "
            f"samples={len(motion_x_q16)}"
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
        "snap_x_q16": round(collision_attributes["ledge_snap_x"] * MELEE_X_TO_SIM_Q16),
        "snap_y_q16": round(collision_attributes["ledge_snap_y"] * MELEE_Y_TO_SIM_Q16),
        "snap_height_q16": round(
            collision_attributes["ledge_snap_height"] * MELEE_Y_TO_SIM_Q16
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
    common_attributes = {
        "initial_walk_velocity_q16": round(
            raw_f32(common_attribute_bits, 0) * MELEE_X_TO_SIM_Q16
        ),
        "walk_acceleration_q16": round(
            raw_f32(common_attribute_bits, 1) * MELEE_X_TO_SIM_Q16
        ),
        "walk_maximum_velocity_q16": round(
            raw_f32(common_attribute_bits, 2) * MELEE_X_TO_SIM_Q16
        ),
        "friction_q16": round(raw_f32(common_attribute_bits, 6) * MELEE_X_TO_SIM_Q16),
        "dash_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 7) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_acceleration_a_q16": round(
            raw_f32(common_attribute_bits, 8) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_acceleration_b_q16": round(
            raw_f32(common_attribute_bits, 9) * MELEE_X_TO_SIM_Q16
        ),
        "dash_run_terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 10) * MELEE_X_TO_SIM_Q16
        ),
        "ground_maximum_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 13) * MELEE_X_TO_SIM_Q16
        ),
        "jump_horizontal_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 15) * MELEE_X_TO_SIM_Q16
        ),
        "jump_vertical_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 16) * MELEE_Y_TO_SIM_Q16
        ),
        "ground_air_jump_momentum_multiplier_q16": q16(
            raw_f32(common_attribute_bits, 17)
        ),
        "jump_horizontal_maximum_velocity_q16": round(
            raw_f32(common_attribute_bits, 18) * MELEE_X_TO_SIM_Q16
        ),
        "shorthop_vertical_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 19) * MELEE_Y_TO_SIM_Q16
        ),
        "air_jump_multiplier_q16": q16(raw_f32(common_attribute_bits, 20)),
        "double_jump_momentum_q16": q16(raw_f32(common_attribute_bits, 21)),
        "double_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 16)
            * raw_f32(common_attribute_bits, 20)
            * MELEE_Y_TO_SIM_Q16
        ),
        "double_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 21) * MELEE_X_TO_SIM_Q16
        ),
        "gravity_q16": round(raw_f32(common_attribute_bits, 23) * MELEE_Y_TO_SIM_Q16),
        "terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 24) * MELEE_Y_TO_SIM_Q16
        ),
        "air_mobility_a_q16": round(
            raw_f32(common_attribute_bits, 25) * MELEE_X_TO_SIM_Q16
        ),
        "air_mobility_b_q16": round(
            raw_f32(common_attribute_bits, 26) * MELEE_X_TO_SIM_Q16
        ),
        "max_aerial_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 27) * MELEE_X_TO_SIM_Q16
        ),
        "air_friction_q16": round(
            raw_f32(common_attribute_bits, 28) * MELEE_X_TO_SIM_Q16
        ),
        "fast_fall_terminal_velocity_q16": round(
            raw_f32(common_attribute_bits, 29) * MELEE_Y_TO_SIM_Q16
        ),
        "maximum_horizontal_air_velocity_q16": round(
            raw_f32(common_attribute_bits, 30) * MELEE_X_TO_SIM_Q16
        ),
        "shield_break_initial_velocity_q16": round(
            raw_f32(common_attribute_bits, 37) * MELEE_Y_TO_SIM_Q16
        ),
        "ledge_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 42) * MELEE_X_TO_SIM_Q16
        ),
        "ledge_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 43) * MELEE_Y_TO_SIM_Q16
        ),
        "wall_jump_horizontal_velocity_q16": round(
            raw_f32(common_attribute_bits, 65) * MELEE_X_TO_SIM_Q16
        ),
        "wall_jump_vertical_velocity_q16": round(
            raw_f32(common_attribute_bits, 66) * MELEE_Y_TO_SIM_Q16
        ),
        "jump_startup_ticks": round(raw_f32(common_attribute_bits, 14)),
        "number_of_jumps": common_attribute_bits[22],
        "turn_duration_ticks": round(raw_f32(common_attribute_bits, 33)),
        "weight": round(raw_f32(common_attribute_bits, 34)),
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
                "falcon_dive_ledge_ecb_capture_sha256": (
                    SPECIALHI_LEDGE_ECB_CAPTURE_SHA256
                ),
                "falcon_dive_ledge_ecb_right_x_melee": (SPECIALHI_ECB_RIGHT_X_MELEE),
                "falcon_dive_ledge_ecb_bottom_y_melee": (SPECIALHI_ECB_BOTTOM_Y_MELEE),
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
                "falcon_airborne_collision_pose_q16": {
                    track_id: tuple(
                        tuple(
                            tuple(frame["ecb_q16"][point])
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
                "falcon_aerial_attack_bottom_y_q16": {
                    track_id: tuple(
                        int(frame["ecb_q16"]["bottom"][1])
                        for frame in track["frames"]
                    )
                    for track_id, track in aerial_attack_tracks.items()
                },
                "falcon_fall_special_collision_pose_melee": {
                    "bottom_y_from_origin": FALL_SPECIAL_ECB_BOTTOM_Y_MELEE,
                },
                "falcon_air_dodge_collision_pose_melee": {
                    "bottom_y_from_origin": AIR_DODGE_ECB_BOTTOM_Y_MELEE,
                },
                "falcon_raptor_boost_hit_air_collision_pose_melee": {
                    "bottom_y_from_origin": (RAPTOR_BOOST_HIT_AIR_ECB_BOTTOM_Y_MELEE),
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
            throw = dict(move["throw"])
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
        f"/* complete Falcon source SHA-256: {complete_source_digest} */",
        f"/* complete 318-submotion catalog SHA-256: {submotion_catalog_digest} */",
        f"/* complete action-script SHA-256: {action_script_digest} */",
        f"/* complete decoded animation-track SHA-256: {animation_tracks_sha256} */",
        "",
        "static const uint8_t pf_m4_falcon_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{EXPECTED_CANONICAL_SHA256[index:index + 2]})"
            for index in range(0, len(EXPECTED_CANONICAL_SHA256), 2)
        ),
        "};",
        "",
        "static const uint8_t pf_m4_falcon_complete_source_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{complete_source_digest[index:index + 2]})"
            for index in range(0, len(complete_source_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t pf_m4_falcon_submotion_catalog_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{submotion_catalog_digest[index:index + 2]})"
            for index in range(0, len(submotion_catalog_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t pf_m4_falcon_action_script_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{action_script_digest[index:index + 2]})"
            for index in range(0, len(action_script_digest), 2)
        ),
        "};",
        "",
        "static const uint8_t pf_m4_falcon_animation_tracks_sha256[32] = {",
        "    "
        + ", ".join(
            f"UINT8_C(0x{animation_tracks_sha256[index:index + 2]})"
            for index in range(0, len(animation_tracks_sha256), 2)
        ),
        "};",
        "",
        "static const pf_m4_falcon_animation_decode_summary",
        "pf_m4_falcon_animation_decode_summary_data = {",
        f"    UINT32_C({animation_node_count}),",
        f"    UINT32_C({animation_track_count}),",
        f"    UINT32_C({animation_key_count}),",
        "};",
        "",
        "static const pf_m4_falcon_submotion_data",
        "pf_m4_falcon_submotions[PF_M4_FALCON_SUBMOTION_COUNT] = {",
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
        "static const pf_m4_falcon_body_collision_timing",
        "pf_m4_falcon_body_collision_timings[PF_M4_FALCON_SUBMOTION_COUNT] = {",
    ])
    lines.extend(
        "    { "
        f"UINT16_C({state_two_frame}), UINT16_C({state_zero_frame}) }},"
        for state_two_frame, state_zero_frame in body_collision_timings
    )
    lines.extend([
        "};",
        "",
        "static const pf_m4_falcon_script_event",
        "pf_m4_falcon_script_events[PF_M4_FALCON_SCRIPT_EVENT_COUNT] = {",
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
        "pf_m4_falcon_script_bytes[PF_M4_FALCON_SCRIPT_BYTE_COUNT] = {",
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
        "pf_m4_falcon_common_attribute_bits[PF_M4_FALCON_COMMON_ATTRIBUTE_COUNT] = {",
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
            "static const pf_m4_falcon_common_attributes",
            "pf_m4_falcon_common_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT16_C({value}),"
            if name.endswith("_ticks") or name in {"number_of_jumps", "weight"}
            else f"    .{name} = INT32_C({value}),"
        )
        for name, value in common_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_ledge_attributes",
            "pf_m4_falcon_ledge_attribute_data = {",
        )
    )
    lines.extend(
        f"    .{name} = INT32_C({value})," for name, value in ledge_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            f"/* qualified ledge-root capture SHA-256: {LEDGE_ROOT_CAPTURE_SHA256} */",
            "static const pf_m4_falcon_ledge_root_positions",
            "pf_m4_falcon_ledge_root_position_data = {",
            "    .catch_frame_one_x_q16 = INT32_C("
            f"{round(LEDGE_CATCH_FRAME_ONE_ROOT_MELEE[0] * MELEE_X_TO_SIM_Q16)}"
            "),",
            "    .catch_frame_one_y_q16 = INT32_C("
            f"{round(-LEDGE_CATCH_FRAME_ONE_ROOT_MELEE[1] * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "    .wait_frame_one_x_q16 = INT32_C("
            f"{round(LEDGE_WAIT_FRAME_ONE_ROOT_MELEE[0] * MELEE_X_TO_SIM_Q16)}"
            "),",
            "    .wait_frame_one_y_q16 = INT32_C("
            f"{round(-LEDGE_WAIT_FRAME_ONE_ROOT_MELEE[1] * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "    .option_frame_one_x_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value[0] * MELEE_X_TO_SIM_Q16)})"
                for value in LEDGE_OPTION_FRAME_ONE_ROOT_MELEE
            )
            + ",",
            "    },",
            "    .option_frame_one_y_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(-value[1] * MELEE_Y_TO_SIM_Q16)})"
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
            "static const pf_m4_falcon_ledge_attack_reference",
            "pf_m4_falcon_ledge_attack_references[2] = {",
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
            "static const int32_t",
            "pf_m4_falcon_hyrule_ledge_jump1_quick_from_wait_q16"
            "[PF_M4_FALCON_LEDGE_JUMP1_QUICK_FRAME_COUNT][2] = {",
            *(
                f"    {{ INT32_C({x}), INT32_C({y}) }},"
                for x, y in LEDGE_JUMP1_HYRULE_QUICK_FROM_WAIT_Q16
            ),
            "};",
            "static const int32_t",
            "pf_m4_falcon_hyrule_ledge_jump1_slow_from_wait_q16"
            "[PF_M4_FALCON_LEDGE_JUMP1_SLOW_FRAME_COUNT][2] = {",
            *(
                f"    {{ INT32_C({x}), INT32_C({y}) }},"
                for x, y in LEDGE_JUMP1_HYRULE_SLOW_FROM_WAIT_Q16
            ),
            "};",
            "static const int32_t",
            "pf_m4_falcon_hyrule_ledge_jump2_frame_one_from_wait_q16[2][2] = {",
            *(
                f"    {{ INT32_C({x}), INT32_C({y}) }},"
                for x, y in LEDGE_JUMP2_HYRULE_FRAME_ONE_FROM_WAIT_Q16
            ),
            "};",
            "",
            "static const pf_m4_falcon_common_special_attributes",
            "pf_m4_falcon_common_special_attribute_data = {",
        )
    )
    lines.extend(
        f"    .{name} = INT32_C({value}),"
        for name, value in common_special_attributes.items()
        if name != "stale_move_slot_reduction_q16"
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_air_dodge_attributes",
            "pf_m4_falcon_air_dodge_attribute_data = {",
            "    .initial_velocity_x_q16 = "
            f"INT32_C({air_dodge_attributes['initial_velocity_x_q16']}),",
            "    .initial_velocity_y_q16 = "
            f"INT32_C({air_dodge_attributes['initial_velocity_y_q16']}),",
            "    .decay_q16 = "
            f"INT32_C({air_dodge_attributes['decay_q16']}),",
            "    .dead_zone = "
            f"UINT16_C({air_dodge_attributes['dead_zone']}),",
            "    .item_throw_window_ticks = "
            f"UINT16_C({air_dodge_attributes['item_throw_window_ticks']}),",
            "    .ordinary_physics_begin_frame = "
            f"UINT16_C({air_dodge_ordinary_physics_begin_frame}),",
            "    .reserved = UINT16_C(0),",
            "};",
            "",
            "static const pf_m4_melee_stale_move_data",
            "pf_m4_melee_stale_move_data_source = {",
            "    .slot_reduction_q16 = {",
            "        "
            + ", ".join(
                f"UINT16_C({value})"
                for value in common_special_attributes["stale_move_slot_reduction_q16"]
            )
            + ",",
            "    },",
            "};",
            "",
            "static const pf_m4_falcon_special_attributes",
            "pf_m4_falcon_special_attribute_data = {",
        )
    )
    lines.extend(
        (
            f"    .{name} = UINT32_C({value}),"
            if name in SPECIAL_UNSIGNED_FIELDS
            else f"    .{name} = INT32_C({value}),"
        )
        for name, value in special_attributes.items()
    )
    lines.extend(
        (
            "};",
            "",
            "static const pf_m4_falcon_neutral_special_timing",
            "pf_m4_falcon_neutral_special_timing_data = {",
            f"    .launch_frame = UINT16_C({specialn_launch_frame}),",
            "    .velocity_scale_begin_frame = "
            f"UINT16_C({specialn_scale_begin_frame}),",
            "    .velocity_scale_end_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame - 1}),",
            "    .ordinary_air_physics_begin_frame = "
            f"UINT16_C({specialn_air_physics_begin_frame}),",
            "};",
            "",
            "static const pf_m4_falcon_side_special_timing",
            "pf_m4_falcon_side_special_timing_data = {",
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
            "static const pf_m4_falcon_up_special_timing",
            "pf_m4_falcon_up_special_timing_data = {",
            "    .air_control_begin_frame = "
            f"UINT16_C({specialhi_ground_control_begin}),",
            "    .throw_gravity_begin_frame = "
            f"UINT16_C({specialhi_throw_gravity_begin}),",
            "    .victim_release_hitstun_ticks = "
            f"UINT16_C({SPECIALHI_CAPTURE_VICTIM_RELEASE_HITSTUN_TICKS}),",
            "    .reserved = UINT16_C(0),",
            "    .grounded_throw_reposition_x_q16 = "
            "INT32_C("
            f"{round(SPECIALHI_GROUNDED_THROW_REPOSITION_X_MELEE * MELEE_X_TO_SIM_Q16)}"
            "),",
            "    .grounded_throw_reposition_y_q16 = "
            "INT32_C("
            f"{round(-SPECIALHI_GROUNDED_THROW_REPOSITION_Y_MELEE * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "};",
            "",
            "static const pf_m4_falcon_down_special_timing",
            "pf_m4_falcon_down_special_timing_data = {",
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
            "    .ground_end_entry_velocity_scale_q16 = "
            f"INT32_C({q16(FALCON_KICK_GROUND_END_ENTRY_VELOCITY_SCALE)}),",
            "};",
            "",
            "static const pf_m4_falcon_collision_pose",
            "pf_m4_falcon_collision_pose_data = {",
            "    .falling_bottom_y_from_origin_q16 = INT32_C("
            f"{round(FALLING_ECB_BOTTOM_Y_MELEE * MELEE_Y_TO_SIM_Q16)}"
            "),",
            "    .down_bound_back_floor_contact_mask = "
            f"UINT32_C({DOWN_BOUND_BACK_FLOOR_CONTACT_MASK}),",
            "    .down_bound_stomach_floor_contact_mask = "
            f"UINT32_C({DOWN_BOUND_STOMACH_FLOOR_CONTACT_MASK}),",
            "    .damage_fly_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in DAMAGE_FLY_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_top_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in DAMAGE_FLY_ECB_TOP_Y_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_side_x_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_X_TO_SIM_Q16)})"
                for value in DAMAGE_FLY_ECB_SIDE_X_MELEE
            )
            + ",",
            "    },",
            "    .damage_fly_side_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in DAMAGE_FLY_ECB_SIDE_Y_MELEE
            )
            + ",",
            "    },",
            "    .air_dodge_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in AIR_DODGE_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .platform_drop_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in PLATFORM_DROP_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .airborne = {",
            *(
                f"        {render_ecb_pose_q16(frame)},"
                for frame in airborne_frames
            ),
            "    },",
            "    .aerial_attack_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({int(frame['ecb_q16']['bottom'][1])})"
                for frame in aerial_attack_frames
            )
            + ",",
            "    },",
            "    .ceiling_bounce = {",
            *(
                f"        {render_ecb_pose_q16(frame)},"
                for frame in ceiling_bounce_frames
            ),
            "    },",
            "    .wall_bounce = {",
            *(
                f"        {render_ecb_pose_q16(frame)},"
                for frame in wall_bounce_frames
            ),
            "    },",
            "    .fall_special_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in FALL_SPECIAL_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .raptor_boost_hit_air_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in RAPTOR_BOOST_HIT_AIR_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "    .falcon_dive_right_x_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_X_TO_SIM_Q16)})"
                for value in SPECIALHI_ECB_RIGHT_X_MELEE
            )
            + ",",
            "    },",
            "    .falcon_dive_bottom_y_from_origin_q16 = {",
            "        "
            + ", ".join(
                f"INT32_C({round(value * MELEE_Y_TO_SIM_Q16)})"
                for value in SPECIALHI_ECB_BOTTOM_Y_MELEE
            )
            + ",",
            "    },",
            "};",
            "",
            "static const pf_m4_reference_hit_phase pf_m4_falcon_hit_phases[] = {",
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
            "static const pf_m4_reference_hit_effect pf_m4_falcon_hit_effects[] = {",
        )
    )
    for effect in effects:
        lines.append(f"    {c_hit_effect(effect)},")
    lines.extend(
        ("};", "", "static const pf_m4_reference_throw pf_m4_falcon_throws[] = {")
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
            "static const pf_m4_reference_move pf_m4_falcon_moves[PF_M4_FALCON_MOVE_COUNT] = {",
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
            "static const int32_t "
            "pf_m4_falcon_translation_x_q16[PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT] = {",
        )
    )
    lines.extend(
        "    "
        + ", ".join(f"INT32_C({value})" for value in motion_x_q16[index : index + 8])
        + ","
        for index in range(0, len(motion_x_q16), 8)
    )
    lines.extend(
        (
            "};",
            "",
            "static const int32_t "
            "pf_m4_falcon_translation_y_q16[PF_M4_FALCON_TRANSLATION_SAMPLE_COUNT] = {",
        )
    )
    lines.extend(
        "    "
        + ", ".join(f"INT32_C({value})" for value in motion_y_q16[index : index + 8])
        + ","
        for index in range(0, len(motion_y_q16), 8)
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
