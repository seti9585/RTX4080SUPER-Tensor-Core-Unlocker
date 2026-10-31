# Corresponding source and retained notices

This source tree accompanies diagnostic dynamic-GOP build B6E0ECEA47B8CA2D8B17C742D3960E60016A792D0B86FA86CEFC13A72A1FCB35.

The retained runtime includes the existing GPL-2.0-only UEFI integration and vendor transaction code, NVIDIA MIT-marked definitions/parsing components, and EDK II headers under their retained notices. See the source headers, deps/edk2/License.txt, and the accompanying LICENSES directory. Original notices in the source have not been replaced by a single package-wide license.

The existing source refers to PORT-NOTES.txt and THIRD_PARTY_NOTICES.md, but those historical documents were absent from the preserved release source. This file and PORT-NOTES.txt are new distribution summaries, not reconstructions presented as the missing originals.

Provenance references recorded in the preserved material:
- TOTORONG/AD103_win_unlocker: existing unlock/recovery integration.
- WildFlash1st/cmp90hx-unlock, revision 76940ae583281daa2b883518a2e89b56c11d3a46, rejoin15 reference: predecessor V67 design and GPL notice, as documented in LICENSES/UPSTREAM-FIRMWARE-NOTICE.md at the distribution root.
- NVIDIA 610.43.03 source archive: metadata, firmware extraction inputs and retained MIT notices for relevant source components. Firmware binaries do not inherit a GPL/MIT license merely because they occur in an archive containing open source code.
- TianoCore EDK II: retained UEFI headers and host decompression reference implementation, with their source notices.

The upstream firmware notice is retained verbatim as historical provenance. Its descriptions of earlier GA102 tooling, old manifest paths or different GPU tests do not describe this package's AD102 generator or broaden its tested hardware coverage.

Corresponding production sources and the exact compiler invocation are included and were rebuilt byte-identically. This verifies build correspondence, not complete third-party rights clearance. Original firmware, extracted GOPs and host-test binary fixtures are deliberately absent. Licensing review before publication remains a separate gate.
