# Reproducible demonstration scenarios

These scenarios define inputs only. They do not contain fabricated benchmark results.

## Base data

- Load `sample-city.json` from **City Map**.
- Load `sample-dispatch.json` from **Emergency Dispatch** after the sample city.
- Load `sample-delivery.json` from **Delivery Optimizer** after the sample city.

## Routing and infrastructure

- Route Finder: Civic Square (`L0`) to Lakeview Homes (`L4`), first by distance and then by travel time.
- Closed-road case: inspect `R6`, which is closed in the supplied city.
- Infrastructure: run Prim and Kruskal on the unchanged construction candidates and compare totals.

## Dispatch

- Use the queued request in `sample-dispatch.json`.
- Dispatch it, then use **Step to next node** until the simulated request completes.

## Delivery and Algorithm Lab

- Load `sample-delivery.json`, keep capacity `7`, and run 0/1 Knapsack before the Fractional demonstration.
- Algorithm Lab: choose **Duplicate-heavy**, size `20`, seed `42`, ascending Merge Sort.

## Number theory and multi-stop tour

- Maintenance intervals: `6, 8, 12`.
- Reachable tour: depot `L0`, stops `L1`, `L2`, and `L5`.
- Unreachable directed case: depot `L0`, destination set containing `L3`; the one-way approach to `L3` has no return route in the sample graph.

## Performance

- Sorting size `250`, seed `42`, one warm-up, seven measured runs.
- Run in the intended submission build mode and export the resulting CSV. Runtime values are generated locally and are intentionally not stored in this repository.
