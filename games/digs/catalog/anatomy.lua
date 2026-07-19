-- SPDX-License-Identifier: GPL-3.0-or-later
-- Flags: vital 1, limb 2. Health values are per-part systemic durability.

vox.define("anatomy", "anatomy.head", {
    order = 400, title = "HEAD", category = "ANATOMY",
    summary = "Vital precision target; ballistic and explosive failure is immediately lethal.",
    detail = "The head is compact and vital. It takes full ballistic damage, high explosive impulse, and modest blunt protection from the miner's helmet. Reaching zero health kills the fighter and emits seeded flesh, blood, helmet, and tool-response particles.",
    tags = "vital,headshot,helmet", flags = 1,
    values = { health = 35, id = 0, sever_threshold = 32 }
})

vox.define("anatomy", "anatomy.torso", {
    order = 401, title = "TORSO", category = "ANATOMY",
    summary = "Primary vital mass and the largest source of health, bleeding, and central blast response.",
    detail = "Torso failure is fatal. It receives central explosion damage, anchors both arms and the head, and produces the highest bleed rate when opened. Heat can cauterize an open torso wound, but enough heat also destroys the part directly.",
    tags = "vital,core,bleeding", flags = 1,
    values = { health = 100, id = 1, sever_threshold = 72 }
})

vox.define("anatomy", "anatomy.pelvis", {
    order = 402, title = "PELVIS", category = "ANATOMY",
    summary = "Central lower-body junction; severe damage destabilizes both legs and movement.",
    detail = "The pelvis is non-vital by itself but supports both thighs. Heavy blunt or explosive damage reduces acceleration and makes knockback harder to recover from. Its wound rate is high and simultaneous leg loss can turn an otherwise survivable hit into bleedout.",
    tags = "core,movement,bleeding", flags = 2,
    values = { health = 70, id = 2, sever_threshold = 60 }
})

vox.define("anatomy", "anatomy.left_upper_arm", {
    order = 403, title = "LEFT UPPER ARM", category = "ANATOMY",
    summary = "Tool-support limb vulnerable to blast separation and heavy cutting impacts.",
    detail = "Damage reduces left-side tool stability. Severing detaches the forearm and hand chain if they remain connected, emits directional blood, and transfers inherited body and impact velocity to the released segments.",
    tags = "limb,arm,tool-control", flags = 2,
    values = { health = 42, id = 3, sever_threshold = 34 }
})

vox.define("anatomy", "anatomy.right_upper_arm", {
    order = 404, title = "RIGHT UPPER ARM", category = "ANATOMY",
    summary = "Dominant tool-support limb vulnerable to blast separation and heavy cutting impacts.",
    detail = "Damage reduces right-side tool stability. Severing detaches the forearm and hand chain if they remain connected, emits directional blood, and transfers inherited body and impact velocity to the released segments.",
    tags = "limb,arm,tool-control", flags = 2,
    values = { health = 42, id = 4, sever_threshold = 34 }
})

vox.define("anatomy", "anatomy.left_forearm", {
    order = 405, title = "LEFT FOREARM", category = "ANATOMY",
    summary = "Narrow limb segment that bleeds quickly after ballistic or shrapnel damage.",
    detail = "The forearm is easier to sever than the upper arm. Loss impairs two-handed weapon stability and releases the left hand with it. A hot cinder or metal fragment may cauterize the wound after opening it.",
    tags = "limb,arm,bleeding", flags = 2,
    values = { health = 32, id = 5, sever_threshold = 25 }
})

vox.define("anatomy", "anatomy.right_forearm", {
    order = 406, title = "RIGHT FOREARM", category = "ANATOMY",
    summary = "Narrow dominant-side segment whose loss sharply impairs aimed tool handling.",
    detail = "The forearm is easier to sever than the upper arm. Loss impairs two-handed weapon stability and releases the right hand with it. A hot cinder or metal fragment may cauterize the wound after opening it.",
    tags = "limb,arm,aim", flags = 2,
    values = { health = 32, id = 6, sever_threshold = 25 }
})

vox.define("anatomy", "anatomy.left_hand", {
    order = 407, title = "LEFT HAND", category = "ANATOMY",
    summary = "Small tool-contact segment susceptible to precision shots and close mining tools.",
    detail = "Hand loss creates a small wound but degrades recoil control and two-handed handling. The detached hand is a light flesh entity and can be moved by water, blasts, and collapsing debris.",
    tags = "limb,hand,tool-control", flags = 2,
    values = { health = 22, id = 7, sever_threshold = 16 }
})

vox.define("anatomy", "anatomy.right_hand", {
    order = 408, title = "RIGHT HAND", category = "ANATOMY",
    summary = "Small dominant tool-contact segment susceptible to precision shots and close mining tools.",
    detail = "Hand loss creates a small wound and sharply degrades tool control. The detached hand is a light flesh entity and can be moved by water, blasts, and collapsing debris.",
    tags = "limb,hand,aim", flags = 2,
    values = { health = 22, id = 8, sever_threshold = 16 }
})

vox.define("anatomy", "anatomy.left_thigh", {
    order = 409, title = "LEFT THIGH", category = "ANATOMY",
    summary = "Heavy locomotion segment with a high bleed rate and strong effect on acceleration.",
    detail = "Thigh damage reduces ground acceleration and jump stability. Severing releases the shin and foot chain, produces a large wound, and can cause rapid bleedout unless the source is cauterized or the fighter dies first.",
    tags = "limb,leg,movement,bleeding", flags = 2,
    values = { health = 55, id = 9, sever_threshold = 44 }
})

vox.define("anatomy", "anatomy.right_thigh", {
    order = 410, title = "RIGHT THIGH", category = "ANATOMY",
    summary = "Heavy locomotion segment with a high bleed rate and strong effect on acceleration.",
    detail = "Thigh damage reduces ground acceleration and jump stability. Severing releases the shin and foot chain, produces a large wound, and can cause rapid bleedout unless the source is cauterized or the fighter dies first.",
    tags = "limb,leg,movement,bleeding", flags = 2,
    values = { health = 55, id = 10, sever_threshold = 44 }
})

vox.define("anatomy", "anatomy.left_shin", {
    order = 411, title = "LEFT SHIN", category = "ANATOMY",
    summary = "Lower locomotion segment exposed to terrain-level shrapnel and crushing debris.",
    detail = "Shin damage lowers run speed and landing control. Loss removes the attached foot and makes sustained movement difficult, but rope and steam still allow a skilled fighter to traverse.",
    tags = "limb,leg,movement", flags = 2,
    values = { health = 38, id = 11, sever_threshold = 29 }
})

vox.define("anatomy", "anatomy.right_shin", {
    order = 412, title = "RIGHT SHIN", category = "ANATOMY",
    summary = "Lower locomotion segment exposed to terrain-level shrapnel and crushing debris.",
    detail = "Shin damage lowers run speed and landing control. Loss removes the attached foot and makes sustained movement difficult, but rope and steam still allow a skilled fighter to traverse.",
    tags = "limb,leg,movement", flags = 2,
    values = { health = 38, id = 12, sever_threshold = 29 }
})

vox.define("anatomy", "anatomy.left_foot", {
    order = 413, title = "LEFT FOOT", category = "ANATOMY",
    summary = "Ground-contact segment controlling traction, braking, and stable landings.",
    detail = "Foot damage weakens braking and wet-surface recovery. Loss reduces traction and makes precise ledge movement harder. Because it is low to the ground, it is frequently struck by shrapnel, lava, and falling material.",
    tags = "limb,foot,traction", flags = 2,
    values = { health = 24, id = 13, sever_threshold = 18 }
})

vox.define("anatomy", "anatomy.right_foot", {
    order = 414, title = "RIGHT FOOT", category = "ANATOMY",
    summary = "Ground-contact segment controlling traction, braking, and stable landings.",
    detail = "Foot damage weakens braking and wet-surface recovery. Loss reduces traction and makes precise ledge movement harder. Because it is low to the ground, it is frequently struck by shrapnel, lava, and falling material.",
    tags = "limb,foot,traction", flags = 2,
    values = { health = 24, id = 14, sever_threshold = 18 }
})
