# Translating

Merkzettel uses gettext through KDE's KI18n. Every language lives in
`po/<language>/merkzettel.po`; the template is `po/merkzettel.pot`. The build
picks up new languages automatically. The layout works with Weblate and other
gettext-based platforms.

## Add a language

1. Create the file from the template, for example for French:

    ```bash
    mkdir -p po/fr
    msginit --no-translator --locale=fr_FR.UTF-8 -i po/merkzettel.pot -o po/fr/merkzettel.po
    ```

2. Translate it with Lokalize, Poedit or a text editor.
3. Check it: `msgfmt -c --statistics po/fr/merkzettel.po`.
4. Build and try it: `LANGUAGE=fr ./build/bin/merkzettel --demo`.
5. Open a pull request.

## Update a translation

When the source strings change, `make pot` regenerates the template and merges
it into every `.po` file. Translate the new and changed (*fuzzy*) entries.
CI fails if the German translation is incomplete.

## Guidelines

- Keep placeholders such as `%1` and `%2`; their order may change.
- Plural forms (`msgid_plural`) follow the language's `Plural-Forms` header.
- Texts with a `msgctxt` describe where the string appears, e.g.
  "Repeat every N …" for the unit names in the recurrence editor.
- Keep the tone of the existing translation; German addresses the user with
  "du".
- Do not translate product names: Merkzettel, Microsoft To Do, Microsoft 365,
  Plasma, Flathub.

The documentation and the website are translated separately as Markdown files
under `docs/<language>/` and `website/src/`.
