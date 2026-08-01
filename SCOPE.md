# Project Vision & Scope: CrossInk-Kobo

The goal of this port is to provide a reliable, source-buildable e-reader firmware for
the Kobo Glo HD N437. It is a separate hardware target, not an Xteink release.

The content below is taken directly from Crosspoint and aligns with CrossInk's vision as well.

## 1. Core Mission

To provide a lightweight, low-power firmware that prioritizes legibility, predictable
sleep/wake behavior and safe local transfer over feature breadth.

## 2. Scope

### In-Scope

*These are features that directly improve the primary purpose of the device.*

* **User Experience:** E.g. User-friendly interfaces, and interactions, both inside the reader and navigating the
  firmware. This includes things like button mapping, book loading, and book navigation like bookmarks.
* **Document Rendering:** E.g. Support for rendering documents (primarily EPUB) and improvements to the rendering
  engine.
* **Format Optimization:** E.g. Efficiently parsing EPUB (CSS/Images) and other documents within the device's
  capabilities.
* **Typography & Legibility:** E.g. Custom font support, hyphenation engines, and adjustable line spacing.
* **E-Ink Driver Refinement:** E.g. Reducing full-screen flashes (ghosting management) and improving general rendering.
* **Library Management:** E.g. Simple, intuitive ways to organize and navigate a collection of books.
* **Local Transfer:** E.g. Simple, "pull" based book loading via a basic web-server or public and widely-used standards.
* **Language Support:** E.g. Support for multiple languages both in the reader and in the interfaces.
* **Reference Tools:** E.g. Local dictionary lookup. Providing quick, offline definitions to enhance comprehension
  without breaking focus.
* **Clock Display (device dependent):**

| Device | Scope |
| -- | -- |
| Kobo Glo HD N437 | RTC and suspend behavior must be validated on the physical device; no wall-clock accuracy claim is made here. |

### Out-of-Scope

*These items are rejected because they compromise the device's stability or mission.*

* **Interactive Apps:** No Notepads, Calculators, or Games. This is a reader, not a PDA.
* **Active Connectivity:** No RSS readers, News aggregators, or Web browsers. Background Wi-Fi tasks drain the battery and complicate the single-core CPU's execution.
* **Media Playback:** No Audio players or Audiobooks.
* **Complex Annotation:** No typed out notes. These features are better suited for devices with better input capabilities and more powerful chips.

### In-scope — Technically Unsupported

*These features align with Crosspoint's goals but are impractical on the current hardware or produce poor UX.*

* **PDF Rendering:** PDFs are fixed-layout documents, so rendering them requires displaying pages as images rather than reflowable text — resulting in constant panning and zooming that makes for a poor reading experience on e-ink.

## 3. Idea Evaluation

While I appreciate the desire to add new and exciting features to Crosspoint Reader, Crosspoint Reader is designed to be a lightweight, reliable, and performant e-reader. Things which distract or compromise the device's core mission will not be accepted. As a guiding question, consider if your idea improve the "core reading experience" for the average user,
and, critically, not distract from that reading experience.

> **Note to Contributors:** If you are unsure if your idea fits the scope, please open a **Discussion** before you start
> coding!
