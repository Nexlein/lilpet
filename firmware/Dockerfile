# ESP-IDF used to build the firmware, locally and in CI (mise run firmware:build): the latest release, pinned here
# only, by its tag and by the digest of the image the tag named then, since a tag can be moved. Dependabot updates both
# in a pull request when a new release is out, and CI checks it before it lands. From a two-part tag such as v6.1 it
# proposes the next minor release (v6.2) only: when a bugfix release is out (v6.1.1), pin it by hand, and Dependabot
# then follows the bugfix releases.
FROM espressif/idf:v6.1@sha256:81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c
