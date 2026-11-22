# Version from file
VERSION := $(shell cat version.txt)

## Check this version can be released (tools/release_check.sh)
.PHONY: check
check:
	tools/release_check.sh

## Tag this version, after the release check (the tag runs the release)
.PHONY: tag
tag: check
	git tag $(VERSION) && git push origin $(VERSION) && \
	echo "Tagged: $(VERSION)"
