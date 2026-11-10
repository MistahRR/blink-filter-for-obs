👉 **[Buy Me a Coffee — buymeacoffee.com/mistah](https://buymeacoffee.com/mistah)**

# Animated Blink (`blink-filter`) 

Ever wanted to make an overlay, a "REC" dot, a text prompt, or a webcam border blink in OBS without building a cursed maze of Move Transition filters or clunky nested scenes? 

Yeah, me too. Honestly, there weren't a thousand ways to fix this—I just had to build a dedicated filter for it.

**Animated Blink** is a lightweight video filter plugin for OBS Studio (30 / 31 / 32) that puts any source—text, image, media, browser, group, or nested scene—on a customizable visibility loop, complete with smooth transitions, custom GPU shader effects, and easing curves.

---

### What does it actually do? (The cool stuff)

Instead of a boring on/off toggle, you can pick independent **Disappear** and **Reappear** animations. 

* **15 Built-in Animations:**
  * **Classics:** None (Instant), Fade, Zoom, Zoom + Fade.
  * **Directional Slides:** Slide from Left, Right, Top, or Bottom.
  * **Shader Wipes:** Wipe from Left, Right, Bottom, or Top.
  * **Retro / Cyberpunk Shaders:** 
    * **Glitch:** Horizontal slice displacement + RGB chromatic aberration split (with adjustable Glitch Strength `%`).
    * **Pixelate:** Dynamic mosaic downscaling during the transition.
    * **Neon Flicker:** Erratic, dying-neon-sign style opacity stuttering.
* **4 Easing Curves:** Linear, Smooth, Ease Out, and Overshoot (Back)—because linear animations feel robotic and we want things to look *snappy*.
* **Full Cycle & Timing Control:**
  * **Visible & Hidden durations:** Set exact times in seconds (animations included). *Pro-tip:* Set Hidden duration to `0` to never hide the source.
  * **Cycle Offset:** Running the filter on 5 different HUD elements and don't want them blinking in creepy unison? Tweak the offset to desynchronize them effortlessly.
  * **Start Hidden** toggle & a **Restart Cycle** button right in the properties to test your timings on the fly.
* **Zero-Cost Performance:** When the source is hidden or static (no active animation), the filter does **nothing** (direct passthrough, zero GPU cost). During an animation, it only adds a single GPU render pass—completely negligible.

---

### Good to know 💡

* **Bounding Box Clipping:** Slide and Zoom animations stay within the source's own bounding box (the content slides and gets clipped at the edges of its zone). If you want a text source to slide across your entire screen, expand its bounding box (e.g., using a **Crop/Pad** filter with negative margins) or apply **Animated Blink** to a **Group**.
* **Audio stays untouched:** For media sources, only the video blinks—the audio is not muted.

---

### Installation (Windows 64-bit — OBS 30 / 31 / 32)

1. Close OBS Studio.
2. Download the latest `.zip` from the **Releases** page and extract/merge its contents into `C:\Program Files\obs-studio\`:
   * `obs-plugins\64bit\obs-blink-filter.dll`
   * `data\obs-plugins\obs-blink-filter\effects\blink.effect`
   * `data\obs-plugins\obs-blink-filter\locale\en-US.ini` & `fr-FR.ini`
3. Relaunch OBS. Right-click any source -> **Filters** -> click **+** under *Effect Filters* -> select **Animated Blink** (or **Clignotement animé**).

---

### Support me ☕


👉 **[Buy Me a Coffee — buymeacoffee.com/mistah](https://buymeacoffee.com/mistah)**

---
---

# 🇫🇷 Version Française — OBS Clignotement animé

Filtre OBS pour n'importe quelle source : texte, image, média, navigateur, groupe ou scène imbriquée. Il fait disparaître puis réapparaître la source en boucle, selon des durées que tu choisis, avec une animation de disparition et une animation de réapparition.

### Installation (Windows 64 bits, OBS 30 / 31 / 32)

1. Ferme OBS.
2. Copie le contenu du zip dans `C:\Program Files\obs-studio\` en fusionnant les dossiers :
   - `obs-plugins\64bit\obs-blink-filter.dll`
   - `data\obs-plugins\obs-blink-filter\effects\blink.effect`
   - `data\obs-plugins\obs-blink-filter\locale\fr-FR.ini` et `en-US.ini`
3. Relance OBS. Fais un clic droit sur la source -> **Filtres** -> **+** sous « Filtres d'effets » -> **Clignotement animé**.

### Réglages & Consommation

* **Durées :** Durée visible, Durée cachée (`0` pour ne jamais cacher), Décalage du cycle (pour désynchroniser plusieurs sources), Commencer caché, et bouton Redémarrer le cycle.
* **Animations :** Aucune, Fondu, Zoom, Zoom + fondu, Glissement (gauche, droite, haut, bas), Volet (gauche, droite, haut, bas), Glitch, Pixelisation, Grésillement (néon).
* **Consommation :** Coût nul (passage direct) quand la source est cachée ou fixe. Un seul rendu GPU négligeable pendant la transition.
* **Bon à savoir :** Les glissements/zooms restent dans le cadre de la source (utilise un Groupe ou un filtre « Rogner / Remplir » pour élargir la zone). Le son d'une source média n'est pas coupé.

☕ **J'adore le café :** **[buymeacoffee.com/mistah](https://buymeacoffee.com/mistah)**
