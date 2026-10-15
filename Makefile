# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Convenience targets. The build itself is plain CMake; see CONTRIBUTING.md.
#   make            configure + build (build/)
#   make test       unit and QML smoke tests (headless)
#   make lint       REUSE, AppStream, desktop file, versions, translations,
#                   accessibility, clang-format, static analysis
#   make pot        regenerate po/merkzettel.pot and merge translations
#   make flatpak    build the Flatpak from the working tree
#   make screenshots  regenerate docs/README/metainfo screenshots (Xvfb + KWin)
#   make docs       documentation (MkDocs) into website/public/docs
#   make run-demo   start the freshly built app with demo data

BUILD ?= build
CLIENT_ID ?=
JOBS ?= $(shell nproc)

.PHONY: all configure build test lint pot flatpak screenshots docs website-deps run-demo clean

all: build

configure:
	cmake -B $(BUILD) -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(if $(CLIENT_ID),-DMERKZETTEL_CLIENT_ID=$(CLIENT_ID))

build: configure
	cmake --build $(BUILD) -j $(JOBS)

test: build
	QT_QPA_PLATFORM=offscreen ctest --test-dir $(BUILD) --output-on-failure

lint: build
	reuse lint
	appstreamcli validate --strict --no-net io.github.mzilinski.merkzettel.metainfo.xml
	desktop-file-validate io.github.mzilinski.merkzettel.desktop
	python3 scripts/check-version.py
	scripts/update-translations.sh --check
	python3 scripts/check-accessibility.py
	git ls-files '*.cpp' '*.h' | xargs clang-format --style=file --dry-run --Werror
	scripts/static-analysis.sh $(BUILD)

pot:
	scripts/update-translations.sh

flatpak:
	flatpak-builder --user --force-clean --install-deps-from=flathub \
	  $(BUILD)/flatpak flatpak/io.github.mzilinski.merkzettel.yml

screenshots: build
	scripts/screenshots.sh $(BUILD)

website-deps:
	cd website && npm ci --no-audit --no-fund

docs: website-deps
	cd website && npm run diagrams
	mkdocs build --strict

run-demo: build
	$(BUILD)/bin/merkzettel --demo

clean:
	rm -rf $(BUILD)
