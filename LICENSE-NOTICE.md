# Licensing status

No project-wide redistribution license has been selected for GRID-ARVx.
Public repository and test distribution are authorized under the creator's
clearance. No resale rights are implied; redistribution remains subject to the
third-party notices and terms below. Do not label this
repository MIT/AGPL/other until the owner selects
the intended terms and checks dependency compatibility.

JUCE's pinned modules offer AGPLv3 or the JUCE 8 agreement. Private, free,
noncommercial testing does not remove dependency license conditions. An
applicable free JUCE tier or compliant open-source route may avoid a fee; the
owner remains responsible for the applicable agreement. The owner selected JUCE
Starter during this project; account enrollment and acceptance are user-controlled.
No paid plan was selected by this agent.

The pinned JUCE version contains an older VST3 SDK with its own license files.
Do not assume a newer SDK's MIT terms apply retroactively to this dependency.
Review its GPL/commercial choices before distributing test binaries to friends.
AU and CLAP dependencies also retain their own notices. The build stages
third-party notices alongside binaries; those notices do not grant rights beyond
the upstream terms.

VST2 is unavailable by default because the user has no authorization. AAX is
conditional: SDK access is subject to Avid's agreement and host testing/signing
requirements. Neither private use nor a renamed format bypasses those terms.
No restricted SDK is included in this archive or downloaded by the workflow.

Official references:

- [JUCE 8 agreement](https://juce.com/legal/juce-8-licence/)
- [Pinned JUCE license](https://github.com/juce-framework/JUCE/blob/8.0.4/LICENSE.md)
- [Pinned VST3 license](https://github.com/juce-framework/JUCE/blob/8.0.4/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt)
- [CLAP adapter](https://github.com/free-audio/clap-juce-extensions)
- [Avid AAX SDK information](https://developer.avid.com/aax/)

Visual Lab Pro belongs to Greyland Audio. A separate unchanged Windows VST3
copy runs locally under `vendor/VelEngine` for the authorized modified
version. The source package includes only the four required
bundle files through an explicit Git allowlist. The private Windows
preview stages its copied engine beside the standalone app; other build targets
still use the independent renderer unless explicitly configured. No user license codes or activation/settings files are included.
See [original engine status](docs/ORIGINAL-ENGINE.md). The permission reported
by the owner is specific to this project; it is not an open-source license and
does not grant resale rights or alter the third-party terms above.
