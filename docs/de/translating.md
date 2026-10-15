# Übersetzen

Merkzettel nutzt gettext über KDEs KI18n. Jede Sprache liegt in
`po/<language>/merkzettel.po`; die Vorlage ist `po/merkzettel.pot`. Der Build
erkennt neue Sprachen automatisch. Die Struktur funktioniert mit Weblate und
anderen gettext-basierten Plattformen.

## Eine Sprache hinzufügen

1. Lege die Datei aus der Vorlage an, zum Beispiel für Französisch:

    ```bash
    mkdir -p po/fr
    msginit --no-translator --locale=fr_FR.UTF-8 -i po/merkzettel.pot -o po/fr/merkzettel.po
    ```

2. Übersetze sie mit Lokalize, Poedit oder einem Texteditor.
3. Prüfe sie: `msgfmt -c --statistics po/fr/merkzettel.po`.
4. Baue und probiere sie aus: `LANGUAGE=fr ./build/bin/merkzettel --demo`.
5. Eröffne einen Pull-Request.

## Eine Übersetzung aktualisieren

Ändern sich die Quelltexte, erzeugt `make pot` die Vorlage neu und gleicht
sie mit jeder `.po`-Datei ab. Übersetze die neuen und geänderten (*fuzzy*)
Einträge. Die CI schlägt fehl, wenn die deutsche Übersetzung unvollständig ist.

## Richtlinien

- Behalte Platzhalter wie `%1` und `%2`; ihre Reihenfolge darf sich ändern.
- Pluralformen (`msgid_plural`) folgen dem Kopfeintrag `Plural-Forms` der
  Sprache.
- Texte mit `msgctxt` beschreiben, wo die Zeichenkette erscheint, z. B.
  „Repeat every N …“ für die Einheiten im Wiederholungseditor.
- Bleib beim Ton der bestehenden Übersetzung; Deutsch spricht die Nutzer mit
  „du“ an.
- Übersetze keine Produktnamen: Merkzettel, Microsoft To Do, Microsoft 365,
  Plasma, Flathub.

Dokumentation und Website werden getrennt als Markdown-Dateien unter
`docs/<language>/` und `website/src/` übersetzt.
