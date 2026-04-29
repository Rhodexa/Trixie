// journal.cpp
// Journal implementation: commit, undo, redo.

#include "journal.h"

void Journal::commit(std::unique_ptr<Command> command) {
    // Erase everything ahead of the cursor — the user has taken a new branch.
    history.erase(history.begin() + cursor, history.end());

    command->execute();
    history.push_back(std::move(command));
    cursor++;
}

void Journal::undo() {
    if (!can_undo()) return;
    cursor--;
    history[cursor]->undo();
}

void Journal::redo() {
    if (!can_redo()) return;
    history[cursor]->execute();
    cursor++;
}

bool Journal::can_undo() const {
    return cursor > 0;
}

bool Journal::can_redo() const {
    return cursor < static_cast<int>(history.size());
}
