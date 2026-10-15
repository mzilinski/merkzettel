// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The use cases of the website; each has a how-to in the docs and a video
// built from docs/videos/<id>.md.
import type { Lang } from "./i18n";

interface UseCase {
  id: string;
  view: string;   // screenshot from docs/assets/screenshots
  text: Record<Lang, { title: string; problem: string; solution: string; result: string; alt: string }>;
}

export const USE_CASES: UseCase[] = [
  {
    id: "work-account",
    view: "login",
    text: {
      de: {
        title: "Dienstkonto auf dem eigenen Linux-PC",
        problem: "Dein Arbeitgeber nutzt Microsoft 365. Auf dem Handy planst du mit Microsoft To Do, auf dem Linux-Desktop bleibt nur die Web-Version im Browser-Tab.",
        solution: "Merkzettel meldet sich mit deinem Arbeits- oder Schulkonto an und zeigt dieselben Listen – nativ auf Plasma, inklusive geteilter Listen.",
        result: "Ein Aufgabenbestand auf Handy, Web und Desktop. Merkzettel fragt nur nach deinen Aufgaben, deinem Namen und dem Angemeldetbleiben.",
        alt: "Anmeldeseite von Merkzettel mit den angefragten Berechtigungen",
      },
      en: {
        title: "Work account on your own Linux PC",
        problem: "Your employer uses Microsoft 365. On the phone you plan with Microsoft To Do; on the Linux desktop there is only the web version in a browser tab.",
        solution: "Merkzettel signs in to your work or school account and shows the same lists – natively on Plasma, including shared lists.",
        result: "One set of tasks on phone, web and desktop. Merkzettel asks only for your tasks, your name and to stay signed in.",
        alt: "Merkzettel's sign-in page listing the permissions it asks for",
      },
    },
  },
  {
    id: "quick-capture",
    view: "quickadd",
    text: {
      de: {
        title: "Aufgaben von überall erfassen",
        problem: "Mitten in einer Mail fällt dir eine Aufgabe ein. Browser-Tab suchen, Liste öffnen, eintippen – und der Gedanke an die Mail ist weg.",
        solution: "Ein globales Tastenkürzel (Meta+Alt+N) holt Merkzettel mit dem Cursor im Eingabefeld nach vorn. „Miete zahlen morgen“ setzt die Fälligkeit gleich mit.",
        result: "Erfassen in drei Sekunden, ohne die Maus anzufassen. Ein Schließen bringt dich zurück, Merkzettel wartet im Systemabschnitt.",
        alt: "Neue Aufgabe „Miete zahlen morgen“ im Eingabefeld",
      },
      en: {
        title: "Capture tasks from anywhere",
        problem: "A task comes to mind in the middle of an e-mail. Finding the browser tab, opening the list, typing it in – and your train of thought is gone.",
        solution: "A global shortcut (Meta+Alt+N) brings Merkzettel up with the cursor in the new-task field. “Pay rent tomorrow” sets the due date as well.",
        result: "Captured in three seconds without touching the mouse. Closing the window takes you back; Merkzettel waits in the system tray.",
        alt: "New task “Pay rent tomorrow” in the input field",
      },
    },
  },
  {
    id: "plan-your-day",
    view: "myday",
    text: {
      de: {
        title: "Den Tag planen",
        problem: "Deine Listen enthalten Dutzende Aufgaben mit und ohne Termin. Morgens willst du nur sehen, was heute zählt.",
        solution: "Abschnitte nach Fälligkeit, die intelligenten Listen Geplant und Wichtig und „Mein Tag“ für genau die Aufgaben, die du dir für heute vornimmst.",
        result: "Eine kurze Liste für heute, abgehakt mit Strg+D. Am nächsten Morgen beginnt „Mein Tag“ leer, Unerledigtes bleibt in seiner Liste.",
        alt: "Die Ansicht „Mein Tag“ mit den Aufgaben für heute",
      },
      en: {
        title: "Plan your day",
        problem: "Your lists contain dozens of tasks with and without dates. In the morning you only want to see what matters today.",
        solution: "Due-date sections, the Planned and Important smart lists, and My Day for exactly the tasks you pick for today.",
        result: "A short list for today, ticked off with Ctrl+D. The next morning My Day starts empty; unfinished tasks stay in their lists.",
        alt: "The My Day view with today's tasks",
      },
    },
  },
  {
    id: "organise-tasks",
    view: "details",
    text: {
      de: {
        title: "Aufgaben organisieren",
        problem: "Manche Aufgaben brauchen mehr als einen Titel: den Beleg für die Reisekostenabrechnung, eine Kategorie, eine monatliche Erinnerung.",
        solution: "Die Detailansicht hält Dateianhänge, Outlook-Kategorien, Wiederholungen und Erinnerungen – synchron mit Microsoft To Do.",
        result: "Alles zur Aufgabe an einem Ort, auf jedem Gerät. Das Suchfeld findet Aufgaben auch über ihre Kategorie.",
        alt: "Detailansicht einer Aufgabe mit Anhängen, Kategorien und Wiederholung",
      },
      en: {
        title: "Organise tasks",
        problem: "Some tasks need more than a title: the receipt for the expense report, a category, a monthly reminder.",
        solution: "The task details hold file attachments, Outlook categories, recurrence and reminders – synchronised with Microsoft To Do.",
        result: "Everything about a task in one place, on every device. The search field finds tasks by category, too.",
        alt: "Task details with attachments, categories and recurrence",
      },
    },
  },
  {
    id: "offline",
    view: "offline",
    text: {
      de: {
        title: "Offline unterwegs",
        problem: "Im Zug oder im Flugzeug ist das Netz unzuverlässig. Die Web-Version zeigt dann gar nichts mehr.",
        solution: "Merkzettel hält eine lokale Kopie aller Listen, startet ohne Netz und verbindet sich später von selbst wieder.",
        result: "Deine Aufgaben sind immer da. Suche, Sortierung und „Mein Tag“ funktionieren auch offline.",
        alt: "Merkzettel offline mit Hinweis und gespeicherten Aufgaben",
      },
      en: {
        title: "Offline on the road",
        problem: "On the train or the plane the connection is unreliable. The web version then shows nothing at all.",
        solution: "Merkzettel keeps a local copy of all lists, starts without a network and reconnects by itself later.",
        result: "Your tasks are always there. Search, sorting and My Day work offline, too.",
        alt: "Merkzettel offline with a banner and the saved tasks",
      },
    },
  },
];
