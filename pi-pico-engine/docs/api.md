# API Documentation

## initEngine
Initializes internal attack tables and loads the starting position.

## parsePosition
Parses a UCI `position` command and updates the board state.

## goCommand
Parses a UCI `go` command and prints the best move to `Serial`.

## think
Performs a search to the requested depth and returns the best `Move`.
