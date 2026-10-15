package engine;

import java.util.Set;

public interface Position {
	
	/**
	 * Indicates whether {@code this} object represents the same position of the obj argument.
	 * @return true if {@code this} object represents the same position of the argument;
	 * false otherwise.
	 */
	boolean equals(Object obj);
	
	int hashCode();
	
	/**
	 * Returns the set of positions that can be reached from {@code this} one through legal moves.
	 * If there are no legal moves, the set must be empty.
	 * @return the set of positions that can be reached from {@code this} one through legal moves.
	 */
	<T extends Position> Set<T> legalMoves();
	
	/**
	 * Returns {@code PLAYER_1} if the attacker has the turn in {@code this} position;
	 * {@code PLAYER_2} otherwise.
	 * @return the player who has the turn in this position.
	 */
	Outcome turn();
	
	/**
	 * If {@code this.legalMoves().isEmpty()}, returns {@code PLAYER_1} if the attacker won
	 * in this position, {@code PLAYER_2} if the defender won, {@code DRAW} otherwise.
	 * 
	 * <p>If {@code !this.legalMoves().isEmpty()}, the result is undefined.
	 * @return the outcome of this position if {@code legalMoves().isEmpty()}.
	 */
	Outcome win();
}