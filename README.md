# ✨ EnchantUnbound

A lightweight mod that breaks the vanilla barriers, allowing you to create the ultimate god-tier gear. No more choosing between power and utility—have it all.

---

### 🛡️ Core Features
* **Ultimate Protection:** Stack Protection, Projectile Protection, Fire Protection, and Blast Protection all on one piece.
* **The God Sword:** Stack **Sharpness**, **Bane of Arthropods**, and **Smite** on a single sword.
* **The God Table:** Enchantment tables now grant these "conflicting" combos naturally.
* **Perfect Bows:** Run **Mending** and **Infinity** together for infinite power that never breaks.
* **True Crossbows:** Combine **Piercing** and **Multishot** for ultimate crowd control.

---

![Showcase Image](assets/image1.jpeg)

---

### ⚠️ Heads Up: Useless Combos
While you *can* put these together now, some enchants just don't work side-by-side. To keep things stable, the mod forces one to override the other:

* **Fortune is disabled by Silk Touch:** You can't get extra drops from a block that hasn't been broken. Silk Touch takes priority, so your Fortune levels won't do anything.
* **Channeling is disabled by Riptide:** You can't strike a mob with lightning if you're busy launching yourself into the air. Riptide wins this round.
* **Loyalty is disabled by Riptide:** There is no point in the trident "returning" to you if you never actually let go of it. 

---

### ⚙️ Total Freedom
If you prefer to have absolutely no restrictions at all—even for the combinations listed above—you can fork the mod and modify `Enchant_isCompatibleWith` in the main.cpp like this :

```c
bool Enchant_isCompatibleWith(void* a1, uint8_t a2) {
    return true;
}
```

## ⚙️ Requirements

- 🚀 [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid)

## 🛠️ Installation

- Install LeviLauncher
- Import AnarchyArray mod in LeviLauncher by doing "Manage Mods > Add Mod"
- Launch Minecraft with the mod activated

## 📜 License
- This project is licensed under the GNU LGPL v3.0.
