````markdown
# Market Strategy — Bipartite Market-Clearing Algorithm

This project implements the **Demange–Gale–Sotomayor (DGS)** ascending-price auction algorithm to compute a **perfect matching** and **equilibrium seller prices** in a bipartite market. The algorithm iteratively adjusts seller prices based on excess demand until no buyer can improve their utility — ensuring a stable, competitive market outcome.

---

##  Project Objective

✔ Practice Python programming with bipartite graph manipulation  
✔ Understand a real-world market-clearing algorithm  
✔ Visualize iterative price adjustments and matchings  
✔ Demonstrate algorithm correctness through interactive plots  

---

##  Installation / Requirements

```bash
pip install networkx matplotlib
````

---

##  How to Run

Basic execution:

```bash
python ./market_strategy.py market.gml
```

With graph visualization:

```bash
python ./market_strategy.py market.gml --plot
```

Interactive mode (shows each round of computation and pricing):

```bash
python ./market_strategy.py market.gml --plot --interactive
```

---

##  Expected GML Format

* 2n nodes:

  * Sellers: `0 .. n-1`
  * Buyers: `n .. 2n-1`
* Required edge attribute:

  * `valuation` → buyer’s value for a seller
* Optional seller attribute:

  * `price` → defaults to `0.0` if missing

Example snippet:

```gml
node [ id 0 price 0.0 ]
node [ id 3 ]
edge [ source 3 target 0 valuation 5.0 ]
```

---

##  Algorithm Overview (DGS Auction)

For each buyer **b** and seller **a**:

**Utility:**
[
u(b,a) = valuation(b,a) - price[a]
]

Each round:
1️ Build the **Preferred Seller Graph (PSG)**
 → connect each buyer to their highest-utility sellers
2️ Compute a **maximum matching**
3️ If all buyers matched  stop
4️ Otherwise:
 • Identify **constricted sellers** (over-demanded)
 • Raise their prices by the minimum ε to change preferences
 • Repeat

The process finds a **Walrasian equilibrium** ensuring fairness and stability.

---

##  Round-by-Round Visualization

Preferred edges are bold. Seller nodes display current price.

### Round 1 — |M| = 1, ε = 1

![Round 1](round1.png)

### Round 2 — |M| = 2, ε = 1

![Round 2](round2.png)

### Round 3 — |M| = 2, ε = 1

![Round 3](round3.png)

### Final — Perfect Matching Found  (|M| = 3)

![Round 4](round4.png)

---

##  Final Results (Text Output Summary)

```
Perfect matching size: 3/3
Matching (buyer -> seller):
 3 -> 0
 4 -> 2
 5 -> 1

Final seller prices:
 seller 0: 3.0000
 seller 1: 1.0000
 seller 2: 0.0000
```

Interpretation:
• Buyers are matched optimally based on valuations
• Sellers with highest demand (0, 1) obtain higher prices
• No buyer can improve utility → **market cleared**

---

