# my-new-project

Ein schlanker Start für ein neues Repository mit korrekt gesetzter **MIT-Lizenz**.

## Inhalt
- `LICENSE` – MIT-Lizenz mit Haftungsausschluss
- `README.md` – Dieses Dokument
- `.gitignore` – Übliche Ignorierregeln für Node, Python, macOS, Linux, Windows

## Schnellstart

```bash
# Lokales Repo initialisieren
git init

# Dateien committen
git add .
git commit -m "Initial commit: MIT license, README, .gitignore"

# Neues Remote-Repo bei GitHub erstellen (mit gh CLI)
# Installiere zuvor: https://cli.github.com/
gh repo create my-new-project --public --source=. --remote=origin --push
# Alternativ im Web: Neues Repo anlegen und dann:
# git remote add origin https://github.com/<USER>/my-new-project.git
# git branch -M main
# git push -u origin main
```
