# Casio Unlocker Linux 🔓

![Logo](Images/logo.png)

**Casio Unlocker Linux** est une application native légère (écrite en C avec GTK3) permettant de **désactiver le Mode Examen** sur les calculatrices Casio (notamment la **Graph 35+E**) directement sous Linux.

Ce logiciel comble un manque, car le logiciel officiel Casio n'est disponible que sur Windows et macOS.

---

## ⚠️ Avertissements Importants

> **Ce logiciel n'est pas officiel.** Il a été développé par rétro-ingénierie du protocole USB Casio. L'utilisation se fait à vos propres risques.

> 🧪 **État du test :** Ce logiciel a été développé et testé avec succès sur **une seule calculatrice physique** (Casio Graph 35+E). Bien que le protocole soit standard, des variations peuvent exister d'un modèle à l'autre.

---

## 📋 Compatibilité

| Modèle | Compatibilité | Remarques |
| :--- | :---: | :--- |
| **Casio Graph 35+E** (USB) | ✅ **Oui** | Cible principale de ce logiciel. |
| **Casio Graph 35+E II** | ℹ️ **Inutile** | Ce modèle fonctionne comme une clé USB (voir section "Informations"). |
| **Autres modèles (90+E, etc.)** | ❓ **Expérimental** | Le logiciel détectera la calculatrice, mais le protocole de déverrouillage peut différer. |

**Système d'exploitation :** Fonctionne sur toute distribution Linux (Debian, Ubuntu, antiX, Fedora, Arch...) disposant des bibliothèques GTK3 et Libusb.

---

## 🛠️ Installation et Compilation

Ce logiciel n'est pas fourni sous forme de paquet `.deb` ou `.rpm`. Vous devez le compiler (c'est très simple).

### 1. Installer les dépendances
Ouvrez un terminal et installez les outils nécessaires (GCC, Make, GTK3, Libusb).

**Sur Debian, Ubuntu, Linux Mint, antiX :**
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev libusb-1.0-0-dev
```

**Sur Arch Linux / Manjaro :**
```bash
sudo pacman -S base-devel gtk3 libusb
```

### 2. Télécharger et Compiler
Placez-vous dans le dossier du projet :
```bash
cd CasioUnlockerLinux
make
```
Cela va créer un fichier exécutable nommé `casio_unlocker`.

---

## 🚀 Utilisation

Pour fonctionner, le logiciel doit avoir un accès direct et total au port USB. Sous Linux, cela nécessite généralement les droits d'administration (root).

1.  **Branchez votre calculatrice** à l'ordinateur.
2.  Sur la calculatrice, appuyez sur **F1** (Transfert Données / USB) lorsqu'elle le demande.
3.  Lancez l'application avec `sudo` :

```bash
sudo ./casio_unlocker
```

4.  Cliquez sur **"Rafraîchir / Scanner"** pour vérifier que la calculatrice est détectée.
5.  Cliquez sur **"DÉCONNECTER (Retirer Mode Examen)"**.
6.  Si le message **"SUCCÈS"** s'affiche, le mode examen est retiré. Vous pouvez débrancher.

---

## ℹ️ Différence entre Graph 35+E et 35+E II

L'application fournit un bouton **"Plus d'infos"** détaillant les procédures :

*   **Graph 35+E (Ancien modèle) :** Nécessite ce logiciel car il utilise un protocole de communication propriétaire complexe.
*   **Graph 35+E II (Nouveau modèle) :** Ne nécessite **pas** ce logiciel. Elle est reconnue comme une clé USB standard sur tous les OS (Linux, Mac, Windows). Il suffit de créer un fichier/dossier dedans et de l'éjecter pour enlever le mode examen.

---

## 🔧 Dépannage

**Erreur : "Impossible d'ouvrir (sudo requis?)"**
*   Linux protège les périphériques USB. Vous devez lancer l'application avec `sudo ./casio_unlocker`.

**Erreur : "Erreur scan USB" ou "Endpoints introuvables"**
*   Vérifiez le câble USB.
*   Assurez-vous d'avoir appuyé sur **F1** sur la calculatrice.
*   Si vous êtes sur une distribution légère (antiX), essayez de désactiver `ModemManager` qui peut interférer : `sudo modprobe -r cdc_acm`.

**Erreur de compilation "fatal error: gtk/gtk.h: No such file..."**
*   Vous avez oublié d'installer `libgtk-3-dev`. Relisez la section Installation.

---

## 📂 Structure du projet

*   `main.c` : Code source principal.
*   `Makefile` : Script de compilation.
*   `Images/` : Dossier contenant le logo et les photos des calculatrices (ne pas supprimer ou renommer).

---

## 📜 Licence

Ce projet est Open Source. Vous êtes libre de le modifier et de le redistribuer.