package enginep1;

import engine.*;
import java.util.*;

public class Engine {
	
	private record Evaluation(boolean win, double depth) implements Comparable<Evaluation> {
		@SuppressWarnings("unchecked")
		private static final HashMap<Double, Evaluation>[] VALUES = new HashMap[2];
		static {
			VALUES[0] = new HashMap<>();
			VALUES[1] = new HashMap<>();
		}
		
		@Override
		public int compareTo(Evaluation o) {
			int cmp = Boolean.compare(this.win, o.win);
			if (cmp != 0)
				return cmp;
			
			return Double.compare(o.depth, this.depth);
		}
		
		public static Evaluation valueOf(boolean win, double depth) {
			int i = win ? 1 : 0;
			Evaluation val = VALUES[i].get(depth);
			if (val == null) {
				val = new Evaluation(win, depth);
				VALUES[i].put(depth, val);
			}
			return val;
		}
	}
	
	private static enum Color { WHITE, GRAY, BLACK }
	
	private Outcome player;
	private HashMap<Position, Color> colors;
	private HashMap<Position, Evaluation> evals;
	private HashMap<Position, HashSet<Position>> dejavu;
	private HashMap<Position, Double> probability;
	
	public Engine(Outcome player) {
		if (player == Outcome.DRAW)
			throw new IllegalArgumentException("Player must be PLAYER_1 or PLAYER_2");

		this.player = player;
		this.colors = new HashMap<>();
		this.evals = new HashMap<>();
		this.dejavu = new HashMap<>();
		this.probability = new HashMap<>();
	}
	
	public void solve(Position p) {
		LinkedList<Position> path = new LinkedList<>();
		if (p.turn() == player) {
			winTree(p, path);
		} else { // Execute the first recursive step outside the recursion
			path.push(p);
			colors.put(p, Color.GRAY);
			
			boolean pWin;
			double pDepth;
			double prob;
			Set<Position> adj = p.legalMoves();
			if (adj.isEmpty()) { // base case
				if (p.win() == player) {
					pWin = true;
					pDepth = 0.0;
					prob = 1.0;
				} else {
					pWin = false;
					pDepth = Double.POSITIVE_INFINITY;
					prob = 0.0;
				}
			} else { // recursive step
				pWin = true;
				pDepth = 0.0;
				prob = 0.0;
				int nVisited = 0;
				
				Iterator<Position> it = adj.iterator();
				while (pWin && it.hasNext()) {
					Position q = it.next();
					Color qColor = colors.get(q);
					boolean win;
					double depth, qProb;
					
					if (qColor != Color.BLACK)
						winTree(q, path);
					
					Evaluation qEval = evals.get(q);
					if (qEval != null) {
						win = qEval.win;
						depth = qEval.depth + 1;
					} else {
						win = false;
						depth = Double.POSITIVE_INFINITY;
					}
					qProb = probability.get(q);
					
					if (!win) {
						pWin = win;
						pDepth = depth;
					} else if (depth > pDepth) {
						pDepth = depth;
					}
					
					prob = prob + qProb;
					nVisited++;
				}
				
				prob = nVisited > 0 ? prob / nVisited : 0.5;
			}
			
			probability.put(p, prob);
			colors.put(p, Color.BLACK);
			evals.put(p, Evaluation.valueOf(pWin, pDepth));
			path.pop();
		}
	}
	
	private void winTree(Position p, LinkedList<Position> path) {
		path.push(p);
		colors.put(p, Color.GRAY);
		dejavu.put(p, new HashSet<>());
		
		Boolean pWin;
		double pDepth;
		double prob;
		Set<Position> adj = p.legalMoves();
		if (adj.isEmpty()) { // base case
			if (p.win() == player) {
				pWin = true;
				pDepth = 0.0;
				prob = 1.0;
			} else {
				pWin = false;
				pDepth = Double.POSITIVE_INFINITY;
				prob = 0.0;
			}
		} else { // recursive step
			if (p.turn() == player) {
				pWin = false;
				pDepth = Double.POSITIVE_INFINITY;
			} else {
				pWin = true;
				pDepth = 0.0;
			}
			prob = 0.0;
			int nVisited = 0;
			
			boolean end = false;
			HashSet<Position> firstPass = new HashSet<>(); // P1 fix: figli BLACK già trattati nel primo ciclo
			Iterator<Position> it = adj.iterator();
			while (!end && it.hasNext()) {
				Position q = it.next();
				if (colors.get(q) == Color.BLACK) {
					firstPass.add(q);
					Evaluation qEval = evals.get(q);
					double depth = qEval.depth + 1;
					
					if (qEval.win != (p.turn() != player)) {
						pWin = qEval.win;
						pDepth = depth;
						end = true;
					} else if (p.turn() != player && depth > pDepth) {
						pDepth = depth;
					}
					
					prob = p.turn() == player
							? Math.max(prob, probability.get(q))
							: prob + probability.get(q);
					nVisited++;
				}
			}
			
			it = adj.iterator();
			while (!end && it.hasNext()) {
				Position q = it.next();
				Color qColor = colors.get(q);
				if (!firstPass.contains(q)) { // P1 fix: legge anche i figli diventati BLACK durante il ciclo
					Boolean win;
					double depth, qProb;
					if (qColor == Color.GRAY) {
						win = null;
						depth = Double.POSITIVE_INFINITY;
						qProb = 0.5;
						
						Position next;
						Iterator<Position> itPath = path.iterator();
						while (!q.equals(next = itPath.next()))
							dejavu.get(next).add(q);
					} else {
						if (qColor == null || qColor == Color.WHITE && dejavu.get(q).retainAll(path))
							winTree(q, path);
						else if (qColor == Color.WHITE) // P2 fix: il salto eredita i bloccanti di q (colpo virtuale, variante A)
							for (Position x : dejavu.get(q)) {
								Iterator<Position> itPath = path.iterator();
								for (Position next = itPath.next(); !x.equals(next); next = itPath.next())
									dejavu.get(next).add(x);
							}
						
						Evaluation qEval = evals.get(q);
						if (qEval != null) {
							win = qEval.win;
							depth = qEval.depth + 1;
						} else {
							win = null;
							depth = Double.POSITIVE_INFINITY;
						}
						qProb = probability.get(q);
					}
					
					if (!Objects.equals(win, p.turn() != player)) {
						pWin = win;
						pDepth = depth;
						if (pWin != null)
							end = true;
					} else if (p.turn() != player && depth > pDepth) {
						pDepth = depth;
					}
					
					prob = p.turn() == player ? Math.max(prob, qProb) : prob + qProb;
					nVisited++;
				}
			}
			
			if (p.turn() != player)
				prob = nVisited > 0 ? prob / nVisited : 0.5;
		}
		
		probability.put(p, prob);
		if (pWin == null) {
			colors.put(p, Color.WHITE);
		} else {
			colors.put(p, Color.BLACK);
			evals.put(p, Evaluation.valueOf(pWin, pDepth));
		}
		path.pop();
	}
	
	public static Position notLosingMove(Position pos, Engine oppEngine) {
		Iterator<Position> it = pos.legalMoves().iterator();
		Position move;
		Evaluation eval;
		do {
			move = it.next();
		} while (oppEngine.colors.get(move) == null
				|| (eval = oppEngine.evals.get(move)) != null && eval.win && it.hasNext());
		
		return move;
	}
	
	public static Position bestMove(Position pos, Engine ownEngine, Engine oppEngine) {
		Iterator<Position> it = pos.legalMoves().iterator();
		Position move = null;
		Data data = null;
		
		do {
			Position next = it.next();
			
			Evaluation ownEval = ownEngine.evals.get(next);
			if (ownEngine.colors.get(next) != null &&
					(oppEngine.colors.get(next) != null || ownEval != null && ownEval.win)) {
				Data nextData = new Data(ownEval, oppEngine.evals.get(next),
						ownEngine.probability.get(next), oppEngine.probability.get(next));
				
				if (move == null || data.compareTo(nextData) < 0) {
					move = next;
					data = nextData;
				}
			}
		} while (it.hasNext());
		
		return move;
	}
	
	private record Data(Evaluation ownEval, Evaluation oppEval, Double ownProb, Double oppProb)
	implements Comparable<Data> {
		
		@Override
		public int compareTo(Data o) {
			Comparator<Evaluation> evalCmp = (e1, e2) -> e1.compareTo(e2);
			
			int cmp = Comparator.nullsFirst(evalCmp).compare(this.ownEval, o.ownEval);
			if (cmp != 0)
				return cmp;
			
			cmp = o.oppEval.compareTo(this.oppEval);
			cmp = Comparator.nullsLast(evalCmp).compare(o.oppEval, this.oppEval);
			if (cmp != 0)
				return cmp;
			
			
			cmp = Double.compare(this.ownProb != null ? this.ownProb : 0.5,
					o.ownProb != null ? o.ownProb : 0.5);
			if (cmp != 0)
				return cmp;
			
			return Double.compare(o.oppProb != null ? o.oppProb : 0.5,
					this.oppProb != null ? this.oppProb : 0.5);
		}
	}
}
