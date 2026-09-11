# Baseline antes de perf/compiler-codegen (commit 3334c0a)

```
seconds      astar  dijkstra  quadtree    octree     hanoi floodfill
zen          0.043     0.166     0.042     0.062     0.057     0.011
lua          0.042     0.158     0.054     0.076     0.067     0.008
wren         0.062     0.331     0.071     0.101     0.074     0.020
zenpy        0.056     0.183     0.051     0.067     0.055     0.011
python       0.069     0.253     0.074     0.110     0.092     0.027

checksums (must be identical down each column):
               astar    dijkstra    quadtree      octree       hanoi   floodfill
zen          3527039     3527039      155484       79081     1310718     4010601
lua          3527039     3527039      155484       79081     1310718     4010601
wren         3527039     3527039      155484       79081     1310718     4010601
zenpy        3527039     3527039      155484       79081     1310718     4010601
python       3527039     3527039      155484       79081     1310718     4010601
```
