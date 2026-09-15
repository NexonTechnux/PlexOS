<p align="center">
  <img src="logo.png" alt="PlexOS" width="160"/>
</p>

<h1 align="center">🖥️ PlexOS</h1>

<p align="center">
  <b>Un sistema operativo scritto da zero in C e Assembly (x86), con GUI grafica, desktop, finestre, menu Start e 6 app.</b><br/>
  <i>Un progetto NexonTech — tutto il kernel, i driver e la GUI sono originali.</i>
</p>

<p align="center">
  <img src="docs/screenshot-720p.png" alt="Desktop PlexOS 720p" width="620"/>
</p>

## ⚠️ Stato: QEMU-first

> **PlexOS è stato testato e ottimizzato su QEMU, dove gira a quasi 60 FPS a 1280x720.**
> L'avvio su hardware fisico reale è ancora in sviluppo (macchine di test: Intel Core i3-1115G4, gestione display Intel UHD — il sistema è vivo anche dove il video non è ancora visibile).
> Perciò: **usa QEMU per provare PlexOS** e goderteli fino in fondo. 😉

## ✨ Cosa fa

- **Boot proprio da zero**: Multiboot (BIOS) + **UEFI** (Multiboot2 + GOP), GDT/IDT, PMM, paging PSE con DMA, heap.
- **GUI grafica completa**: desktop con wallpaper, doppio buffer + vsync (zero flicker), fine mouse PS/2, finestre, menu Start con animazioni, barra delle applicazioni, multitasking round-robin.
- **Drivers scritti a mano**: VBE framebuffer, Bochs dispi, tastiera e mouse PS/2, ATA PIO, PCI, RTC, timer PIT, **rete Intel e1000** (QEMU `-net nic,model=e1000`), rilevamento CPU (SSE, PAT, TSC), probe display Intel (Tiger Lake).
- **File system VFS** con partizione integrata e `initrd`-like immagini disco.
- **Shell di sistema** con **più di 70 comandi**, tra cui: `ls cd pwd cat cp mv rm mkdir touch tree find grep head tail wc du df free ps kill stat mount dmesg history cal fortune sleep date uptime uname lscpu cpuinfo meminfo pci lsusb netinfo netstat dmesg framebuffer theme sysinfo plexfetch halt reboot poweroff ...`
- **6 app**:
  - 🗔 **Terminale** — con emulatore ANSI e comandi.
  - 📂 **Gestione File** — esploratore del file system.
  - 🧮 **Calcolatrice** — con motore di calcolo delle espressioni.
  - 🌐 **Browser** — navigatore testuale intergovernomenale.
  - 📝 **Editor** — editor di testo a schermo intero.
  - 📊 **SysMon** — monitor di sistema con grafica.
- **Impostazioni** (tema chiaro/scuro/blu, internet on/off) nel menu Start.
- **Diagnostica "cieca"** per real hardware: indicatori a pieno schermo colorati + lampeggi del CAPS LOCK (fino a 11 codici) per capire dove si ferma il boot quando il video ancora non si accende.

## 🔨 Build

Serve (su Debian/Ubuntu):

```bash
sudo apt install gcc-multilib nasm xorriso grub-pc-bin mtools qemu-system-x86
```

Poi:

```bash
make
```

Produce `plexos.iso` (~12 MB), direttamente avviabile da QEMU **e da USB** (GRUB) tramite `cp plexos.iso /dev/sdX && sync` su una chiavetta.

## ▶️ Eseguire su QEMU

```bash
make run
```

oppure, non-interattivo:

```bash
qemu-system-i386 -cdrom plexos.iso -m 256M -vga std \
  -drive id=disk0,if=ide,format=raw,file=/dev/shm/plexos_disk.img \
  -net nic,model=e1000 -net user -rtc base=localtime
```

> Il disco `plexos_disk.img` è il file system che PlexOS monta. Senza disco la shell mostra solo una partizione di memoria (`/ram`).

## ⌨️ Inside la GUI

- Menu Start: clic sull'icona in alto a sinistra (o `Alt+S`).
- Finestre: drag con il titolo, chiudi con la ✕.
- `Ctrl+Alt+P` = screenshot, `Alt+Tab` = anticipo finestra, `Alt+Q` = chiudi app.
- Nella shell: `help` per l'elenco completo, `plexfetch` per la pagina sistema, `theme 0/1/2` per cambiare tema, `netinfo` per lo stato della rete.

## 🧠 Architettura (in breve)

```
boot/    boot.asm (loader multiboot 32-bit)
kernel/  gdt idt pmm paging heap timer cpu power mb2
drivers/ serial fb keyboard mouse pci ata rtc gpu e1000 bochs intel_disp
fs/      vfs        shell (CLI ~70 comandi)
gui/     gui        (finestre, menu, wallpaper)
apps/    terminal   fileman   calc   browser   editor   sysmon
lib/     string     printf    font
```

Screenshot UEFI 1920x1080 (GOP):

<p align="center"><img src="docs/screenshot-1080p.png" alt="PlexOS UEFI 1080p" width="620"/></p>

<p align="center"><img src="docs/screenshot-startmenu.png" alt="Menu Start" width="620"/></p>

## ⚖️ Licenza

PlexOS è un progetto didattico/hobby di **NexonTech**. Riuso libero citando la provenienza.