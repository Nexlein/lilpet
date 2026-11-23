# ESP-IDF used to build the firmware, locally and in CI (mise run firmware:build): the latest release, pinned here
# only, by its tag and by the digest of the image the tag named then, since a tag can be moved. Renovate updates both
# in a pull request when a new release is out, and CI checks it before it lands: from v6.1.1, the bugfix releases
# (v6.1.2) and the next minor one (v6.2), then its bugfix releases (v6.2.1), never a beta, rc or dev tag
# (.github/renovate.json5).
FROM espressif/idf:v6.1.1@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91
