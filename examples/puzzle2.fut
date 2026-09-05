# Example 5x5 Futoshiki puzzle -- medium difficulty.
#
# Format (one directive per line, '#' starts a comment):
#   fixed <row> <col> <value>   a given cell
#   gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)
#
# All coordinates are 0-based (top-left cell is 0 0).

fixed 0 0 4
fixed 2 2 3
fixed 4 4 2

gt 0 0 0 1
gt 1 4 0 4
gt 2 1 2 0
gt 3 2 3 3
gt 4 0 4 1
