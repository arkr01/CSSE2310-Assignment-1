#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "push2310.h"

int main(int argc, char** argv) {
    // Initialise major variables and validate arguments
    ExitCodes exitStatus = EXIT_NORMAL;
    Game* game = (Game*)malloc(sizeof(Game)); 
    if (argc_player_types_handler(argc, argv, &exitStatus, game)) {
	return exitStatus;
    }

    // Validate and setup game file
    FILE* gameFile = fopen(argv[3], "r");
    char* rowsAndColumns = 0;
    char* rowErrors = 0;
    char* columnErrors = 0;
    /* If EOF found in invalid location (e.g. player has typed a valid move
     * yet triggered EOF), use this flag to handle this */
    int eofFlag = 0;
    if (file_setup(gameFile, game, &exitStatus, &rowsAndColumns,
	    &rowErrors, &columnErrors, &eofFlag)) {
	return exitStatus;
    }

    /* Validate and set up game board. Represented as multi-dimensional
     * dynamic array with each element as a char */ 
    init_board(game, gameFile, &eofFlag);
    if (file_contents_error_handler(game, rowErrors, columnErrors,
	    &exitStatus)) {
	free(rowsAndColumns);
	fclose(gameFile);
	game_free_memory(game);
	return exitStatus;
    }

    if (check_board_full(game)) {
	free(rowsAndColumns);
	game_free_memory(game);
	fprintf(stderr, "Full board in load\n");
	fclose(gameFile);
	exitStatus = EXIT_FULL_BOARD;
	return exitStatus;
    }

    // Play the game, handle end of file on stdin when input required
    play_game(game, &exitStatus, rowsAndColumns, gameFile, &eofFlag);
    if (exitStatus == EXIT_EOF) {
	return exitStatus;
    }

    // Clean up after game is over, and calculate/display winner(s)
    game_over(game, rowsAndColumns, gameFile); 
    return exitStatus;
}

int argc_player_types_handler(int argc, char** argv, ExitCodes* exitStatus,
	Game* game) {

    // Check correct number of args
    if (argc != 4) {
	free(game);
	fprintf(stderr, "Usage: push2310 typeO typeX fname\n"); 
	*exitStatus = EXIT_ARGS;
	return *exitStatus;
    }
    
    // Assign player types
    game->playerTypeO = *argv[1];
    game->playerTypeX = *argv[2];

    // flags for valid player types
    char validPlayerTypes[3] = {'0', '1', 'H'};
    int firstCheck = 0;
    int secondCheck = 0;

    /* Iterate through set of valid player types and check if the player types
     * input by the user match */
    for (int i = 0; i < 3; i++) {
	if (game->playerTypeO == validPlayerTypes[i]) {
	    firstCheck = 1;
	}

	if (game->playerTypeX == validPlayerTypes[i]) {
	    secondCheck = 1;
	}
    }

    if (!(firstCheck && secondCheck)) {
    	free(game);
	fprintf(stderr, "Invalid player type\n");
	*exitStatus = EXIT_PLAYER_TYPE;
	return *exitStatus;
    }
    return *exitStatus;
}

int file_setup(FILE* gameFile, Game* game, ExitCodes* exitStatus,
	char** rowsAndColumns, char** rowErrors,
	char** columnErrors, int* eofFlag) {
    // Check if fopen failed (returns NULL on failure)
    if (!gameFile) {
	free(game);
	fprintf(stderr, "No file to load from\n");
	*exitStatus = EXIT_FILE_READ;
	return *exitStatus;
    }
    // Get the rows and columns from the first line of the file, check spacing
    *rowsAndColumns = read_line(gameFile, 81, eofFlag);
    if (space_counter(rowsAndColumns) != 1) {
	game->rows = game->columns = 1; // Sentinel value
	return *exitStatus;
    }

    /* Continue reading the file only if there is information still available
     * to be be read */
    if (!(*eofFlag) && strcmp(*rowsAndColumns, "test_EOF")) {
	
	/* Split rowsAndColumns via a space (via strtok), and then convert
	 * them to long ints (via strtol) */	
	game->rows = strtol(strtok(*rowsAndColumns, " "), rowErrors, 10);
	
	// Ensure there actually is a column dimension specified
	char* columnsVerify = strtok(NULL, " ");
	if (columnsVerify != NULL) {
	    game->columns = strtol(columnsVerify, columnErrors, 10);
	} else {
	    game->columns = 1; // Sentinel value
	}
	/* Second line of file identifies first/current player read_line
	 * returns malloc'd char*, dereference to get first char and free (did
	 * not use fgetc to ensure the next file read avoids storing the new
	 * line immediately after the current player character) */
	char* currentPlayerRead = read_line(gameFile, 1, eofFlag);
	
	/* Verifies current player is only 1 letter (further verification in
	 * file_contents_error_handler) */
	if (strlen(currentPlayerRead) == 1) {
	    game->currentPlayer = *currentPlayerRead;
	} else {
	    game->currentPlayer = 'f'; // Sentinel value
	}
	/* Ensure no invalid frees if EOF is found (as read_line free's the
	 * char* it malloc'd to store the line to be read if EOF is found) */
	if (!(*eofFlag) && strcmp(currentPlayerRead, "test_EOF")) {
	    free(currentPlayerRead);
	}
    }
    return *exitStatus;
}

int file_contents_error_handler(Game* game, char* rowErrors,
	char* columnErrors, ExitCodes* exitStatus) { 
    // Initialise counters to check for invalid file contents
    int borderZeroCounter = 0;
    int interiorZeroCounter = 0;
    int invalidCharCounter = 0;
    int r, c;

    /* Iterate through each cell in game board, each cell has two chars, hence
     * we must double the columns to iterate through each individual cell */
    for (r = 0; r < game->rows; r++) {
	for (c = 0; c < game->columns * 2; c++) {
	    // Check interior for zeros and invalid chars
	    validate_interior(r, c, game, &interiorZeroCounter,
		    &invalidCharCounter); 

	    // Count number of zeros in border and check for invalid chars
	    validate_border(r, c, game, &borderZeroCounter,
		    &invalidCharCounter);
	    
	    /* Ensure corners have nothing but blank spaces, use
	     * borderZeroCounter as sentinel otherwise */
	    if ((r == 0 || r == game->rows - 1) &&
		    (c == 0 || c == 1 || c == LAST_CELL_COLUMN ||
		    c == SCORE_COLUMN_OF_LAST_CELL)) {
		if (game->board[r][c] != ' ') {
		    borderZeroCounter = -1;
		}
	    }
	}
    }

    /* Check for incorrect dimensions, incorrect player, and invalid chars
     * borderZeroCounter: 1st/last rows have (game->columns - 2) zeros
     * (subtract 2 due to corners having two spaces), the other rows have 2
     * zeros (1st/last cells in each row), hence:
     * borderZeroCounter == 2 * (game->columns - 2) + 2 * (game->rows - 2),
     * simplifying the above expression resuls in the expression below */
    if (game->rows < 3 || game->columns < 3 || game->currentPlayer !=
	    toupper(game->currentPlayer) ||
	    borderZeroCounter != 2 * (game->rows + game->columns) - 8 ||
	    interiorZeroCounter != 0 || r != game->rows ||
	    c != (game->columns) * 2 || invalidCharCounter != 0 ||
	    (game->currentPlayer != 'X' && game->currentPlayer != 'O') ||
	    *rowErrors != '\0' || *columnErrors != '\0') {
	fprintf(stderr, "Invalid file contents\n");
	*exitStatus = EXIT_FILE_CONTENTS;
    }
    return *exitStatus;
}

void validate_interior(int r, int c, Game* game, int* interiorZeroCounter,
	int* invalidCharCounter) {
    // Ensure indexing the interior values of the board
    if (r != 0 && r != game->rows - 1 && c != 0 && c !=
	    SCORE_COLUMN_OF_LAST_CELL) {
	if (game->board[r][c] == '0') {
	    (*interiorZeroCounter)++;
	} else if (!(c % 2 || isdigit(game->board[r][c])) ||
		((c % 2) && game->board[r][c] != '.' &&
		game->board[r][c] != 'O' &&
		game->board[r][c] != 'X')) {
	    /* If even column, ensure cells are numbers (even as indexing from
	     * 0, so first cell will be a score value). If odd column, ensure
	     * cell is either empty (i.e. has a .) or occupied by a player's
	     * stone */
	    (*invalidCharCounter)++;
	}
    }
}

void validate_border(int r, int c, Game* game, int* borderZeroCounter,
	int* invalidCharCounter) {
    
    // Count zeros in top and bottom row
    if (r == 0 || r == game->rows - 1) {
	if (game->board[r][c] == '0') {
	    (*borderZeroCounter)++;
	} else if ((!(c % 2 || isdigit(game->board[r][c])) ||
		((c % 2) && game->board[r][c] != '.' &&
		game->board[r][c] != 'O' &&
		game->board[r][c] != 'X')) && c > 1 &&
		c < SCORE_COLUMN_OF_LAST_CELL) {
	    /* If even column, ensure cells are numbers (even as indexing from
	     * 0, so first cell will be a score value). If odd column, ensure
	     * cell is either empty (i.e. has a .) or occupied by a player's
	     * stone */
	    (*invalidCharCounter)++;
	}
    } else if (c == 0 || c == SCORE_COLUMN_OF_LAST_CELL) {
	/* Count zeros in first and last column that aren't in the top and
	 * bottom row (else if ensures corners are avoided) */
	if (game->board[r][c] == '0') {
	    (*borderZeroCounter)++;
	} else if (!(c % 2 || isdigit(game->board[r][c])) ||
		((c % 2) && game->board[r][c] != '.' &&
		game->board[r][c] != 'O' &&
		game->board[r][c] != 'X')) {
	    /* If even column, ensure cells are numbers (even as indexing from
	     * 0, so first cell will be a score value). If odd column, ensure
	     * cell is either empty (i.e. has a .) or occupied by a player's
	     * stone */
	    (*invalidCharCounter)++;
	}
    }
}

int space_counter(char** input) {
    int spaceCount = 0;
    
    // Iterate through string (end identified by finding the null terminator)
    for (int i = 0; (*input)[i] != '\0'; i++) {
	if((*input)[i] == ' ') {
	    spaceCount++;
	}
    }
    return spaceCount;
}

char* read_line(FILE* file, size_t size, int* eofFlag) {
    char* result = (char*)malloc(sizeof(char) * size);
    int position = 0;
    int next = 0;

    while(1) {
	next = fgetc(file);
	
	/* Used to differentiate between when there is nothing but EOF to be
	 * read and when there is information to be read (e.g. a human move)
	 * and then EOF occurs */
	if (next == EOF && position == 0) {
	    free(result);
	    return "test_EOF"; // Sentinel value
	} else if (next == '\n' || next == EOF) {
	    // Re-allocate if size too much/little
	    if (position != size - 1) {
		result = (char*)realloc((void*)result, position + 1);
	    }
	    if (next == EOF) {
		*eofFlag = 1;
	    }
	    result[position] = '\0';
	    return result;
	} else {
	    result[position++] = (char)next;
	}
    }
}

void init_board(Game* game, FILE* gameFile, int* eofFlag) {
    game->board = (char**)malloc(game->rows * sizeof(char*));

    for (int r = 0; r < game->rows; r++) {
	/* Each cell for the score character and the playing character
	 * (i.e. an X, O, or a .), hence we must read in 2 * game->columns
	 * worth of characters, as well as one more for the null character
	 * as we are storing a string (NOTE: the null character is not present
	 * in the save game format, only for the internal representation of
	 * the game. */
	game->board[r] =
		read_line(gameFile, 2 * (game->columns) + 1, eofFlag);
	
	/* Handle incorrect number of rows specified in file by mallocing
	 * meaningless space that will get freed regardless. This is to
	 * prevent extra invalid frees that will cause memory problems. These
	 * invalid dimensions are handled in file_contents_handler */
	if (!strcmp(game->board[r], "test_EOF") && game->rows != r) {
	    game->board[r] = 
		    (char*)malloc(sizeof(char) * 2 * (game->columns) + 1);
	}
    }
    /* Any EOF issues are handled elsewhere, this is simply present in the
     * function as read_line requires it as a parameter */
    *eofFlag = 0;
}

int check_board_full(Game* game) {
    /* Iterate through the interior cells of the board, searching for an empty
     * cell */
    for (int r = 1; r < game->rows - 1; r++) {
	for (int c = 3; c < SCORE_COLUMN_OF_LAST_CELL; c++) {
	    if (game->board[r][c] == '.') {
		return EXIT_NORMAL;
	    }
	}
    }
    return EXIT_FULL_BOARD;
}

void play_game(Game* game, ExitCodes* exitStatus, char* rowsAndColumns,
	FILE* gameFile, int* eofFlag) {
    // Print the game board
    for (int r = 0; r < game->rows; r++) {
	printf("%s\n", game->board[r]);
    }
    
    char* move = 0;
    char* rowMoveErrors = 0;
    char* columnMoveErrors = 0;

    // Play game until interior is full, at this point the game is over
    do {
	// Calculate the appropriate move
	game_move(&move, &rowMoveErrors, &columnMoveErrors, game,
		exitStatus, eofFlag);
	/* Handle EOF on stdin when input required. The necessary stderr
	 * message is handled already in game_move in the situation of a valid
	 * human move followed by EOF. */
	if (!strcmp(move, "test_EOF")) { 
	    if (!(*eofFlag)) {
		fprintf(stderr, "End of file\n");	
	    }
	    free(rowsAndColumns);
	    game_free_memory(game);
	    fclose(gameFile);
	    return;
	}

	/* To check for an automated move after processing game_move, the
	 * move variable is set to the sentinel value of "automated". In the
	 * rare event that the user types "automated" as their human move,
	 * ensure to free this invalid move, as the sentinel value for
	 * "automated" exists to ensure that invalid free's do not occur. */
	if(!strcmp(rowMoveErrors, "automatedHuman")) {
	    free(move);
	    continue;
	}

	// Execute the move decided by game_move
	play_move(rowMoveErrors, columnMoveErrors, game);

	/* Only free human moves as automated moves do not allocate memory to
	 * process a move from stdin (i.e. from the user) */
	if (strcmp(move, "automated")) {
	    free(move);
	}
    } while(!check_board_full(game));
}

void game_move(char** move, char** rowMoveErrors, char** columnMoveErrors,
	Game* game, ExitCodes* exitStatus, int* eofFlag) {

    /* Type 0 Moves - Ensure this only executes if the current player is a
     * type 0 player. */
    if ((game->currentPlayer == 'O' && game->playerTypeO == '0') ||
	    (game->currentPlayer == 'X' && game->playerTypeX == '0')) {
	type_zero_move(move, rowMoveErrors, columnMoveErrors, game);
	return;
    }
     
    /* Type 1 Moves - Ensure this only executes if the current player is a
     * type 1 player. */
    char opponent = (game->currentPlayer == 'X') ? 'O' : 'X';
    if ((game->currentPlayer == 'O' && game->playerTypeO == '1') ||
	    (game->currentPlayer == 'X' && game->playerTypeX == '1')) {
	type_one_move(game, opponent, move, rowMoveErrors,
		columnMoveErrors);
	return;
    }

    /* Human Moves - As the player types have already been validated, checking
     * if the current player is of type H is unnecessary as automated player
     * moves have been handled and the function will not reach this point if
     * the current player is automated. Below either prompts the user for
     * their next move, or fprints the appropriate message to stderr if a
     * valid move was followed by EOF. */
    if (!(*eofFlag)) {
	printf("%c:(R C)> ", game->currentPlayer);
    } else {
	fprintf(stderr, "%c:(R C)> %s", game->currentPlayer, "End of file\n"); 
    }
    *move = read_line(stdin, 81, eofFlag);
    
    if (!strcmp(*move, "test_EOF")) {	
	*exitStatus = EXIT_EOF;
	return;
    }

    // Process and validate human move (this includes saving)
    human_move(move, game, rowMoveErrors, columnMoveErrors, eofFlag); 
}

void play_move(char* rowMoveErrors, char* columnMoveErrors, Game* game) {
    /* Check strtol invalid inputs, out of bound human moves, and ensures
     * chosen cell is empty */
    if (*rowMoveErrors == '\0' && *columnMoveErrors == '\0' &&
	    game->rowMove > 0 && game->rowMove < game->rows - 1 &&
	    game->columnMove > 0 &&
	    game->columnMove < game->columns - 1 &&
	    game->board[game->rowMove][MOVE_INDEX] == '.') {	
	game->board[game->rowMove][MOVE_INDEX] =
		game->currentPlayer;
	
	// Check if current player is automated, display move
	if ((game->currentPlayer == 'O' && game->playerTypeO != 'H') ||
		(game->currentPlayer == 'X' &&
		game->playerTypeX != 'H')) {
	    printf("Player %c placed at %ld %ld\n", game->currentPlayer,
		    game->rowMove, game->columnMove);
	}
	
	// Print the board
	for (int r = 0; r < game->rows; r++) {
	    printf("%s\n", game->board[r]);
	}

	// Swap player for next move
	game->currentPlayer = (game->currentPlayer == 'X') ? 'O' : 'X';
    } else if (*rowMoveErrors == '\0' && *columnMoveErrors == '\0' &&
	    ((game->rowMove == 0 || game->rowMove == game->rows - 1) !=
	    (game->columnMove == 0 || game->columnMove ==
	    game->columns - 1)) &&
	    game->board[game->rowMove][MOVE_INDEX] == '.') {
	/* Above condition with != acts as logical XOR. Ensures that if move
	 * is in the border, it is not in a corner */
	
	/* Check if move is in border and cell is empty, handles pushing cell
	 * moves */
	push_move(game);
    }
}

void type_zero_move(char** move, char** rowMoveErrors,
	char** columnMoveErrors, Game* game) {
    /* Handle player O type 0 moves; iterate through the board top to bottom,
     * left to right, until an empty cell is found. */
    if (game->currentPlayer == 'O' && game->playerTypeO == '0') {
	for (int r = 1; r < game->rows - 1; r++) {
	    for (int c = 3; c < SCORE_COLUMN_OF_LAST_CELL; c++) {
		if (game->board[r][c] == '.') {
		    /* Set *move to "automated" so that play_move does not
		     * free the move variable (it otherwise free's the move
		     * variable as it is malloc'd by read_line for human
		     * moves) */
		    *move = "automated";
		    game->rowMove = r; 
		    game->columnMove = COLUMN_MOVE;
		    
		    /* Set errors to == '\0' so that play_move can process the
		     * move */
		    *rowMoveErrors = *columnMoveErrors = "";
		    return;
		}
	    }
	}
    } else if(game->currentPlayer == 'X' && game->playerTypeX == '0') {
	/* Handle player X type 0 moves; iterate through the board bottom to
	 * top, right to left, until an empty cell is found. */
	for (int r = game->rows - 2; r > 0; r--) {
	    for (int c = LAST_INTERIOR_CELL_COLUMN; c > 2; c--) {
		if (game->board[r][c] == '.') {
		    // See the player O type 0 comments
		    *move = "automated";
		    game->rowMove = r;
		    game->columnMove = COLUMN_MOVE;
		    *rowMoveErrors = *columnMoveErrors = "";
		    return;
		}
	    }
	}
    }
}

void type_one_move(Game* game, char opponent, char** move,
	char** rowMoveErrors, char** columnMoveErrors) {
    /* This conditional essentially checks each type one move in the order
     * specified in the spec until a valid move is found. */
    if (type_one_push_down(game, opponent, move, rowMoveErrors,
	    columnMoveErrors)) {
	return;
    } else if (type_one_push_left(game, opponent, move,
	    rowMoveErrors, columnMoveErrors)) {
	return;
    } else if (type_one_push_up(game, opponent, move,
	    rowMoveErrors, columnMoveErrors)) {
	return;
    } else if (type_one_push_right(game, opponent, move,
	    rowMoveErrors, columnMoveErrors)) {
	return;
    } else {
	type_one_highest_cell(game, move, rowMoveErrors,
		columnMoveErrors);
    }
}

int type_one_push_down(Game* game, char opponent, char** move,
	char** rowMoveErrors, char** columnMoveErrors) {
    int scoreCol, scorePush, r, c;
    for (c = 3; c < SCORE_COLUMN_OF_LAST_CELL; c += 2) {
	scoreCol = 0;
	scorePush = 0;
	// Ensure pushing rules are met
	if (game->board[0][c] != '.' ||
		game->board[1][c] == '.' ||
		game->board[game->rows - 1][c] != '.') {
	    continue;
	}
	
	/* Calculate score of both players that is contributed to by the cells
	 * in a specific column, and check what these scores would be if a
	 * pushing cells move was made */
	for (r = 0; r < game->rows - 2; r++) {
	    if (game->board[r + 1][c] == opponent) {
		scoreCol += atoi(&game->board[r + 1][c - 1]);
		scorePush += atoi(&game->board[r + 2][c - 1]); 
	    } else if (game->board[r + 1][c] == '.') {
		break;
	    }
	}
	if (scorePush < scoreCol) {
	    /* Set *move to "automated" so that play_move does not free the
	     * move variable (it otherwise free's the move variable as it is
	     * malloc'd by read_line for human moves) */
	    *move = "automated";

	    game->rowMove = 0;
	    game->columnMove = COLUMN_MOVE;
		    
	    // Set errors to == '\0' so that play_move can process the move
	    *rowMoveErrors = *columnMoveErrors = "";
	    return 1;
	}
    }
    return 0;
}

int type_one_push_left(Game* game, char opponent, char** move,
	char** rowMoveErrors, char** columnMoveErrors) {
    int scoreCol, scorePush, r, c;
    for (r = 1; r < game->rows - 1; r++) {
	scoreCol = 0;
	scorePush = 0;
	// Ensure pushing rules are met
	if (game->board[r][LAST_CELL_COLUMN] != '.' ||
		game->board[r][LAST_INTERIOR_CELL_COLUMN] == '.' ||
		game->board[r][1] != '.') {
	    continue;
	}

	/* Calculate score of both players that is contributed to by the cells
	 * in a specific row, and check what these scores would be if a
	 * pushing cells move was made */
	for (c = LAST_CELL_COLUMN; c > 4; c -= 2) {
	    if (game->board[r][c - 2] == opponent) {
		scoreCol += atoi(&game->board[r][c - 3]);
		scorePush += atoi(&game->board[r][c - 5]);
	    } else if (game->board[r][c - 2] == '.') {
		break;
	    }
	}	
	if (scorePush < scoreCol) {
	    /* Set *move to "automated" so that play_move does not free the
	     * move variable (it otherwise free's the move variable as it is
	     * malloc'd by read_line for human moves) */
	    *move = "automated";

	    game->rowMove = r;
	    game->columnMove = game->columns - 1;
	    
	    // Set errors to == '\0' so that play_move can process the move 
	    *rowMoveErrors = *columnMoveErrors = "";
	    return 1;
	}
    }
    return 0;
}

int type_one_push_up(Game* game, char opponent, char** move,
	char** rowMoveErrors, char** columnMoveErrors) {
    int scoreCol, scorePush, r, c;
    for (c = LAST_INTERIOR_CELL_COLUMN; c > 2; c -= 2) {
	scoreCol = 0;
	scorePush = 0;
	// Ensure pushing rules are met
	if (game->board[game->rows - 1][c] != '.' ||
		game->board[game->rows - 2][c] == '.' ||
		game->board[0][c] != '.') {
	    continue;
	}
	
	/* Calculate score of both players that is contributed to by the cells
	 * in a specific column, and check what these scores would be if a
	 * pushing cells move was made */
	for (r = game->rows - 1; r > 1; r--) {
	    if (game->board[r - 1][c] == opponent) {
		scoreCol += atoi(&game->board[r - 1][c - 1]);
		scorePush += atoi(&game->board[r - 2][c - 1]);
	    } else if (game->board[r - 1][c] == '.') {
		break;
	    }
	}

	if (scorePush < scoreCol) {
	    /* Set *move to "automated" so that play_move does not free the
	     * move variable (it otherwise free's the move variable as it is
	     * malloc'd by read_line for human moves) */
	    *move = "automated";
	    game->rowMove = game->rows - 1;
	    game->columnMove = COLUMN_MOVE;

	    // Set errors to == '\0' so that play_move can process the move
	    *rowMoveErrors = *columnMoveErrors = "";
	    return 1;
	}
    }
    return 0;
}

int type_one_push_right(Game* game, char opponent, char** move,
	char** rowMoveErrors, char** columnMoveErrors) {
    int scoreCol, scorePush, r, c;
    for (r = game->rows - 2; r > 0; r--) {
	scoreCol = 0;
	scorePush = 0;
	// Ensure pushing rules are met
	if (game->board[r][1] != '.' ||
		game->board[r][3] == '.' ||
		game->board[r][LAST_CELL_COLUMN] != '.') {
	    continue;
	}

	/* Calculate score of both players that is contributed to by the cells
	 * in a specific column, and check what these scores would be if a
	 * pushing cells move was made */
	for (c = 1; c < SCORE_COLUMN_OF_LAST_INTERIOR_CELL; c += 2) {
	    if (game->board[r][c + 2] == opponent) {
		scoreCol += atoi(&game->board[r][c + 1]);
		scorePush += atoi(&game->board[r][c + 3]);
	    } else if (game->board[r][c + 2] == '.') {
		break;
	    }
	}

	if (scorePush < scoreCol) {
	    /* Set *move to "automated" so that play_move does not free the
	     * move variable (it otherwise free's the move variable as it is
	     * malloc'd by read_line for human moves) */
	    *move = "automated";
	    game->rowMove = r;
	    game->columnMove = 0;
	    
	    // Set errors to == '\0' so that play_move can process the move
	    *rowMoveErrors = *columnMoveErrors = "";
	    return 1;
	}
    }
    return 0;
}

void type_one_highest_cell(Game* game, char** move, char** rowMoveErrors,
	char** columnMoveErrors) {
    int currentScoreO, currentScoreX, maxRow, maxColumn;
    current_score_calc(&currentScoreO, &currentScoreX, game); 
    
    int maxScore = 0;
    if (game->board[1][3] == '.') {
        maxScore = atoi(&game->board[1][2]); // Set to first cell if empty
    }
    // Iterate through board interior and find highest valued cell
    for (int r = 1; r < game->rows - 1; r++) {
        for (int c = 2; c < SCORE_COLUMN_OF_LAST_CELL; c += 2) {
	    if (game->board[r][c + 1] == '.' && maxScore <
		    atoi(&game->board[r][c])) {
		maxRow = r;
		maxColumn = c;
		maxScore = atoi(&game->board[maxRow][maxColumn]);
		if (currentScoreO == currentScoreX) {
		    /* Set *move to sentinel value so that play_move can
		     * handle invalid free (human move needs malloc) */
		    *move = "automated";
		    game->rowMove = maxRow;
		    /* Each cell contains 2 chars, the score and the player */
		    game->columnMove = maxColumn / 2;
		    /* Set errors to '/0' so that play_move processes *move */
		    *rowMoveErrors = *columnMoveErrors = "";
		    return;
		}
	    }
	}
    }
    // For case when tied but all cells of equal value
    if (currentScoreO == currentScoreX) {
        for (int r = 1; r < game->rows - 1; r++) {
	    for (int c = 2; c < SCORE_COLUMN_OF_LAST_CELL; c += 2) {
		if (game->board[r][c + 1] == '.') {
		    *move = "automated";
		    game->rowMove = r;
		    game->columnMove = c / 2;
		    *rowMoveErrors = *columnMoveErrors = "";
		    return;
		}
	    }
	}
    }
    // This last block handles normal situation without ties
    *move = "automated";
    game->rowMove = maxRow;
    game->columnMove = maxColumn / 2;
    *rowMoveErrors = *columnMoveErrors = "";
    return;
}

void human_move(char** move, Game* game, char** rowMoveErrors,
	char** columnMoveErrors, int* eofFlag) {
    // Ensure exactly one space is in move
    int spaceCounter = space_counter(move);

    // Check for blank moves, invalid chars, leading and trailing spaces/tabs
    char* endOfMove = *move + strlen(*move) - 1;
    if (endOfMove < *move || (**move != 's' &&
	    (**move == ' ' || !isdigit(*endOfMove)
	    || **move == '\t' ||
	    spaceCounter != 1))) {
	// Sentinel values to ensure move is considered invalid
	game->rowMove = -1;
	game->columnMove = -1;
	
	/* Set errors to == '\0' so that play_move can process and handle the
	 * invalid move */
	*rowMoveErrors = *columnMoveErrors = "";
	if (!strcmp(*move, "automated")) {
	    *rowMoveErrors = "automatedHuman";
	}

	/* Ensure appropriate formatting if a valid move that was previously
	 * processed ended in EOF */
	if (*eofFlag) {
	    printf("\n");
	}
	return;
    }

    // Saving - if not saving process row and column moves
    if (**move != 's') {
	game->rowMove = strtol(strtok(*move, " "), rowMoveErrors, 10);
	game->columnMove = strtol(strtok(NULL, " "), columnMoveErrors, 10);
	if (*eofFlag) {
	    printf("\n");
	}
    } else {
	save_game(rowMoveErrors, columnMoveErrors, move, game);
    }
}

void current_score_calc(int* scoreO, int* scoreX, Game* game) {
    *scoreO = 0;
    *scoreX = 0;
    for (int r = 0; r < game->rows; r++) {
	/* As each char is represented as its own element in the internal game
	 * representation, each column has both a score cell and a playing
	 * cell, we must double the number of columns to index the end of the
	 * board */
	for (int c = 0; c < game->columns * 2; c++) {
	    if (game->board[r][c] == 'O') {
		*scoreO += atoi(&game->board[r][c - 1]);
	    } else if (game->board[r][c] == 'X') {
		*scoreX += atoi(&game->board[r][c - 1]);
	    }
	}
    }
}

void push_move(Game* game) {
    int emptyCellCounter = 0;
    int r, c;
    r = c = 0;

    /* Check if move is in edge and there is a stone immediately next to said
     * edge cell to be pushed. Below checks if move is in bottom edge, top
     * edge, right edge, and then left edge. */
    if (game->rowMove == game->rows - 1 &&
	    game->board[game->rows - 2][MOVE_INDEX] != '.') {
	push_up(game, emptyCellCounter, r);
    } else if (game->rowMove == 0 &&
	    game->board[1][MOVE_INDEX] != '.') {
	push_down(game, emptyCellCounter, r);
    } else if (game->columnMove == game->columns - 1 &&
	    game->board[game->rowMove][LAST_INTERIOR_CELL_COLUMN] != '.') {
	push_left(game, emptyCellCounter, c);
    } else if (game->columnMove == 0 &&
	    game->board[game->rowMove][2 * (game->columnMove + 1) + 1] !=
	    '.') {
	push_right(game, emptyCellCounter, c);
    }
}

void push_up(Game* game, int emptyCellCounter, int r) {
    // Ensure column isn't full
    for (r = game->rows - 3; r >= 0; r--) {
        if (game->board[r][MOVE_INDEX] == '.') {
	    emptyCellCounter++;
	    break;
	}
    }

    /* If column isn't full, go to first empty cell and set it equal to the
     * value of the cell below it, effectively pushing all cells in column
     * upwards */
    if (emptyCellCounter) {
        for (int row = r; row < game->rows - 2; row++) {
	    game->board[row][MOVE_INDEX] =
		    game->board[row + 1][MOVE_INDEX];
	}
	
	// Last cell must have value of player who made pushing cell move
	game->board[game->rows - 2][MOVE_INDEX] =
		game->currentPlayer;

	// If automated player (i.e. type 1) pushed cell, display move
	if ((game->currentPlayer == 'O' && game->playerTypeO != 'H') ||
		(game->currentPlayer == 'X' && game->playerTypeX != 'H')) {
	    printf("Player %c placed at %ld %ld\n", game->currentPlayer,
		    game->rowMove, game->columnMove);
	}

	// Print board
	for (r = 0; r < game->rows; r++) {
	    printf("%s\n", game->board[r]);
	}

	// Swap current player for next move
	game->currentPlayer = (game->currentPlayer == 'X') ? 'O' : 'X';
    }
}

void push_down(Game* game, int emptyCellCounter, int r) {
    // Ensure column isn't full
    for (r = 2; r < game->rows; r++) {
        if (game->board[r][MOVE_INDEX] == '.') {
	    emptyCellCounter++;
	    break;
	}
    }

    /* If column isn't full, go to first empty cell and set it equal to the
     * value of the cell above it, effectively pushing all cells in column
     * downwards */
    if (emptyCellCounter) {
        for (int row = r; row > 1; row--) {
	    game->board[row][MOVE_INDEX] =
		    game->board[row - 1][MOVE_INDEX];
	}
	
	// Last cell must have value of player who made pushing cell move
	game->board[1][MOVE_INDEX] = game->currentPlayer;

	// If automated player (i.e. type 1) pushed cell, display move
	if ((game->currentPlayer == 'O' && game->playerTypeO != 'H') ||
		(game->currentPlayer == 'X' && game->playerTypeX != 'H')) {
	    printf("Player %c placed at %ld %ld\n", game->currentPlayer,
		    game->rowMove, game->columnMove);
	}

	// Print board
	for (r = 0; r < game->rows; r++) {
	    printf("%s\n", game->board[r]);
	}

	// Swap current player for next move
	game->currentPlayer = (game->currentPlayer == 'X') ? 'O' : 'X';
    }
}

void push_left(Game* game, int emptyCellCounter, int c) {
    // Ensure row isn't full
    for (c = SECOND_LAST_INTERIOR_CELL_COLUMN; c >= 0; c--) {
        if (game->board[game->rowMove][c] == '.') {
	    emptyCellCounter++;
	    break;
	}
    }

    /* If row isn't full, go to first empty cell and set it equal to the value
     * of the cell right of it, effectively pushing all cells in row to the
     * left */
    if (emptyCellCounter) {
        for (int col = c; col < LAST_INTERIOR_CELL_COLUMN; col += 2) {
	    game->board[game->rowMove][col] =
		    game->board[game->rowMove][col + 2];
	}

	// Last cell must have value of player who made pushing cell move
	game->board[game->rowMove][LAST_INTERIOR_CELL_COLUMN] =
		game->currentPlayer;

	// If automated player (i.e. type 1) pushed cell, display move
	if ((game->currentPlayer == 'O' && game->playerTypeO != 'H') ||
		(game->currentPlayer == 'X' && game->playerTypeX != 'H')) {
	    printf("Player %c placed at %ld %ld\n", game->currentPlayer,
		    game->rowMove, game->columnMove);
	}

	// Print board
	for (int r = 0; r < game->rows; r++) {
	    printf("%s\n", game->board[r]);
	}

	// Swap current player for next move
	game->currentPlayer = (game->currentPlayer == 'X') ? 'O' : 'X';
    }
}

void push_right(Game* game, int emptyCellCounter, int c) {
    // Ensure row isn't full - start at 2 cells after the move (i.e. 4 chars)
    for (c = MOVE_INDEX + 4; c < 2 * game->columns; c++) {
        if (game->board[game->rowMove][c] == '.') {
	    emptyCellCounter++;
	    break;
	}
    }

    /* If row isn't full, go to first empty cell and set it equal to the value
     * of the cell left of it, effectively pushing all cells in row to the
     * right */
    if (emptyCellCounter) {
	// Push right until cell immediately before the player's cell move
        for (int col = c; col > MOVE_INDEX + 2; col -= 2) {
	    game->board[game->rowMove][col] =
		    game->board[game->rowMove][col - 2];
	}

	// Last cell must have value of player who made pushing cell move
	game->board[game->rowMove][MOVE_INDEX + 2] =
		game->currentPlayer;

	// If automated player (i.e. type 1) pushed cell, display move
	if ((game->currentPlayer == 'O' && game->playerTypeO != 'H') ||
		(game->currentPlayer == 'X' && game->playerTypeX != 'H')) {
	    printf("Player %c placed at %ld %ld\n", game->currentPlayer,
		    game->rowMove, game->columnMove);
	}

	// Print board
	for (int r = 0; r < game->rows; r++) {
	    printf("%s\n", game->board[r]);
	}

	// Swap current player for next move
	game->currentPlayer = (game->currentPlayer == 'X') ? 'O' : 'X';
    }
}

void save_game(char** rowMoveErrors, char** columnMoveErrors, char** move,
	Game* game) {
    // Set below to sentinel values for play_move to process
    game->rowMove = game->columnMove = 0;
    *rowMoveErrors = *columnMoveErrors = "";

    // Ensure simply 's' isn't input
    if (strlen(*move) > 1) {
	FILE* saveFile = fopen(*move + 1, "w");

	// Check if fopen failed to open the fail
	if (!saveFile) {
	    fprintf(stderr, "Save failed\n");
	    return;
	}
	    
	// Write to save file
	fprintf(saveFile, "%ld %ld\n", game->rows, game->columns);
	fprintf(saveFile, "%c\n", game->currentPlayer);
	for (int r = 0; r < game->rows; r++) {
	    fprintf(saveFile, "%s\n", game->board[r]);
	}
	fflush(saveFile);
	fclose(saveFile);
    }
}

void game_over(Game* game, char* rowsAndColumns, FILE* gameFile) {
    int scoreO, scoreX;

    current_score_calc(&scoreO, &scoreX, game);

    // Assign current player to be the winner
    game->currentPlayer = (scoreX > scoreO) ? 'X' : 'O';

    // Handle case of tie
    if (scoreO == scoreX) {
        printf("Winners: O X\n");
    } else {
        printf("Winners: %c\n", game->currentPlayer);
    }
    
    free(rowsAndColumns);
    game_free_memory(game);
    fclose(gameFile);
}

void game_free_memory(Game* game) {
    for (int r = 0; r < game->rows; r++) {
	free(game->board[r]);
    }

    free(game->board);
    free(game);
}
