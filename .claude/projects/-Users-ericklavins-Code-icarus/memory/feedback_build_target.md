---
name: preferred_build_target
description: User only tests with make sim-uefi64 and 64-bit UEFI for real hardware. Don't suggest or test other build targets.
type: feedback
---

User only uses `make sim-uefi64` for testing and `./util/build-efi64` + `./util/make-usb-img` for real hardware. Don't suggest `make sim`, `make sim-fb`, or `make sim-uefi` — those are legacy targets.

**Why:** The user has moved fully to UEFI 64-bit. The other builds exist but aren't actively used.

**How to apply:** When building/testing, always use `make sim-uefi64`. When making changes, ensure the UEFI64 build works — don't worry about verifying the other targets.
