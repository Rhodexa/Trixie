// journal.h
// The undo/redo system.
//
// Every change to project state is a Command. Nothing mutates state directly.
// The Journal owns all executed commands and can walk backwards (undo) or
// forwards (redo) through them.
//
// When a new command is committed after an undo, the "future" is discarded —
// Trixie uses a linear history, not a tree.

#pragma once

#include <memory>
#include <string>
#include <vector>

// Base class for every user action.
// Subclasses implement execute() and undo() as a matched pair.
struct Command {
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;

    std::string description; // shown in the undo history panel later
};

class Journal {
public:
    // Calls command->execute() and takes ownership of the command.
    // Discards any undone commands that existed ahead of the cursor.
    void commit(std::unique_ptr<Command> command);

    void undo();
    void redo();

    bool can_undo() const;
    bool can_redo() const;

private:
    std::vector<std::unique_ptr<Command>> history;
    int cursor = 0; // index of the next empty slot past the last committed command
};
