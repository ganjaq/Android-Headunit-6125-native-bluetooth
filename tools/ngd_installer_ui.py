"""Local PL/EN desktop interface. No telemetry, account or admin approval service."""
from pathlib import Path
import contextlib
import json
import os
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import ngd_patch as ngd
from ngd_public_installer import Installer, VERSION, find_adb


class QueueWriter:
    def __init__(self, events): self.events = events
    def write(self, value):
        if value.strip(): self.events.put(('log', value.rstrip()))
        return len(value)
    def flush(self): pass


class Window:
    def __init__(self, root):
        self.root, self.events, self.engine, self.inputs, self.busy = root, queue.Queue(), None, None, False
        root.title('NEXTGENDRIVE — Bluetooth ADB Installer '+VERSION)
        root.geometry('940x760')
        root.minsize(800, 650)
        root.protocol('WM_DELETE_WINDOW', self.close)
        frame = ttk.Frame(root, padding=20)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text='Natywny Bluetooth / Native Bluetooth', font=('Segoe UI', 20)).pack(anchor='w')
        ttk.Label(frame, text='Qualcomm 6125 • Android 10 • A2DP / HID / AVRCP', font=('Segoe UI', 11)).pack(anchor='w', pady=(4, 12))
        ttk.Label(frame, text='1. Włącz debugowanie USB i Device/ADB. Podłącz tylko jedno radio.\n'
                             '2. Wskaż sprawdzoną paczkę. Sprawdź radio, potem zatwierdź instalację.\n'
                             'Enable USB debugging and Device/ADB; connect one radio, choose a reviewed kit, then check.',
                  wraplength=850).pack(anchor='w')
        fields = ttk.Frame(frame)
        fields.pack(fill='x', pady=14)
        fields.columnconfigure(1, weight=1)
        executable_dir = Path(sys.executable).parent if getattr(sys, 'frozen', False) else ngd.BASE
        default_release = executable_dir/'release'
        self.adb = tk.StringVar(value=find_adb())
        self.release = tk.StringVar(value=str(default_release) if default_release.exists() else '')
        self.profile = tk.StringVar(value='')
        for row, (label, value, folder) in enumerate([
            ('ADB (adb.exe)', self.adb, False), ('Paczka / Release folder', self.release, True),
            ('Profil bliźniaka / Twin profile (optional)', self.profile, False)]):
            ttk.Label(fields, text=label).grid(row=row, column=0, sticky='w', pady=5)
            ttk.Entry(fields, textvariable=value).grid(row=row, column=1, sticky='ew', padx=10)
            ttk.Button(fields, text='Wybierz / Browse', command=lambda v=value, d=folder: self.browse(v, d)).grid(row=row, column=2)
            value.trace_add('write', self.invalidate)
        self.use_root = tk.BooleanVar(value=True)
        ttk.Checkbutton(frame, text='Uzyskaj root przez ADB przy sprawdzaniu (restartuje tylko połączenie ADB) / Request ADB root',
                        variable=self.use_root, command=self.invalidate).pack(anchor='w')
        self.status = tk.StringVar(value='Gotowy do sprawdzenia / Ready to check')
        ttk.Label(frame, textvariable=self.status, font=('Segoe UI', 11, 'bold'), wraplength=850).pack(anchor='w', pady=12)
        actions = ttk.Frame(frame)
        actions.pack(fill='x')
        self.buttons = []
        for text, action in [('Sprawdź radio / Check', self.check), ('Zainstaluj / Install', self.install),
                             ('Cofnij / Roll back', self.rollback), ('Restart i kontrola / Restart & check', self.restart)]:
            b = ttk.Button(actions, text=text, command=action)
            b.pack(side='left', padx=(0, 8), pady=5)
            self.buttons.append(b)
        self.buttons[1].configure(state='disabled')
        self.text = tk.Text(frame, height=15, wrap='word', state='disabled', font=('Consolas', 10))
        self.text.pack(fill='both', expand=True, pady=10)
        ttk.Label(frame, text='Bez konta i zgody Admina. Backup lokalny; restart wyłącznie po potwierdzeniu.\n'
                             'No account or remote approval. Local backups; restart requires confirmation.\n'
                             'Pliki OEM nie są dołączone / OEM payload is not included.', wraplength=850).pack(anchor='w')
        self.data = Path(os.environ.get('LOCALAPPDATA', str(Path.home())))/'NEXTGENDRIVE/BluetoothInstaller'
        self.root.after(100, self.poll)

    def browse(self, variable, directory):
        if self.busy: return
        path = filedialog.askdirectory() if directory else filedialog.askopenfilename()
        if path: variable.set(path)

    def invalidate(self, *args):
        self.engine, self.inputs = None, None
        if hasattr(self, 'buttons'): self.buttons[1].configure(state='disabled')

    def parameters(self):
        if not self.adb.get().strip(): raise RuntimeError('Wskaż oficjalny adb.exe / Select official adb.exe')
        return (self.adb.get().strip(), self.release.get().strip() or str(ngd.BASE/'release'), self.profile.get().strip() or None, self.use_root.get())

    def new_engine(self):
        adb, release, profile, root = self.parameters()
        return Installer(adb, release, self.data, profile=profile, root=root)

    def log(self, text):
        self.text.configure(state='normal')
        self.text.insert('end', text+'\n')
        self.text.see('end')
        self.text.configure(state='disabled')

    def work(self, fn, label):
        if self.busy: return
        self.busy = True
        self.status.set(label)
        for button in self.buttons: button.configure(state='disabled')
        def run():
            try:
                with contextlib.redirect_stdout(QueueWriter(self.events)):
                    result = fn()
                self.events.put(('result', result))
            except Exception as exc:
                self.events.put(('error', str(exc)))
            finally:
                self.events.put(('done', None))
        threading.Thread(target=run, daemon=True).start()

    def check(self):
        try:
            self.inputs = self.parameters()
            self.engine = self.new_engine()
            engine = self.engine
        except Exception as exc:
            messagebox.showerror('NEXTGENDRIVE', str(exc)); return
        self.work(engine.check, 'Sprawdzanie / Checking…')

    def install(self):
        try:
            if self.inputs != self.parameters() or not self.engine or not self.engine.checked:
                raise RuntimeError('Sprawdź radio ponownie / Check the radio again')
        except Exception as exc:
            messagebox.showerror('NEXTGENDRIVE', str(exc)); return
        if not messagebox.askyesno('Instalacja / Installation',
                'Potwierdzasz backup i instalację systemowej modyfikacji Bluetooth?\n'
                'Fabryczny BLINK zostanie zatrzymany przy następnym starcie; funkcje zależne mogą przestać działać.\n'
                'Musisz mieć sprawdzony sposób odzyskania radia. Nie będzie automatycznego restartu.\n\n'
                'Confirm backup and system modification, whole BLINK pause, and a verified recovery method.\n'
                'OEM Bluetooth-dependent functions may stop working. No automatic restart.'):
            return
        engine = self.engine
        self.work(lambda: engine.install(accepted=True), 'Backup i instalacja / Backup and installation…')

    def rollback(self):
        if not messagebox.askyesno('Cofnięcie / Rollback',
                'Przywrócić OEM na podstawie zachowanej lokalnej instalacji?\n'
                'Wymagana odpowiadająca jej paczka. Dane i kopie zostają; restart osobno.\n\n'
                'Prepare OEM return using the matching kit and saved local baseline? Backups remain; restart separately.'):
            return
        try: engine = self.new_engine()
        except Exception as exc:
            messagebox.showerror('NEXTGENDRIVE', str(exc)); return
        self.invalidate()
        self.work(lambda: engine.rollback(accepted=True), 'Cofanie / Preparing rollback…')

    def restart(self):
        if not messagebox.askyesno('Restart',
                'Uruchomić ponownie Androida? Muzyka i ADB zostaną przerwane.\n'
                'USB może wrócić do Host — wtedy ustaw Device/ADB.\n\n'
                'Restart Android? Music and ADB will disconnect. Switch back to Device/ADB if needed.'):
            return
        try: engine = self.new_engine()
        except Exception as exc:
            messagebox.showerror('NEXTGENDRIVE', str(exc)); return
        self.invalidate()
        self.work(lambda: engine.restart(accepted=True), 'Restart — w razie potrzeby ustaw Device/ADB / Restart — restore Device/ADB if needed…')

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == 'log': self.log(value)
                elif kind == 'result':
                    self.log(json.dumps(value, ensure_ascii=False, indent=2) if isinstance(value, dict) else value)
                    self.status.set(value.get('status', 'Gotowe / Done') if isinstance(value, dict) else 'Gotowe / Done')
                elif kind == 'error':
                    self.status.set('Zatrzymano / Stopped')
                    self.log(value)
                    messagebox.showerror('NEXTGENDRIVE', value)
                elif kind == 'done':
                    self.busy = False
                    for button in self.buttons: button.configure(state='normal')
                    try: ready = bool(self.engine and self.engine.checked and self.inputs == self.parameters())
                    except RuntimeError: ready = False
                    if not ready:
                        self.buttons[1].configure(state='disabled')
        except queue.Empty: pass
        self.root.after(100, self.poll)

    def close(self):
        if self.busy:
            messagebox.showinfo('NEXTGENDRIVE', 'Poczekaj na zakończenie operacji / Wait for the operation to finish')
        else: self.root.destroy()


def launch():
    root = tk.Tk()
    Window(root)
    root.mainloop()
