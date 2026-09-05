# Example 5x5 Futoshiki puzzle -- constraints only, no givens.
#
# Format (one directive per line, '#' starts a comment):
#   fixed <row> <col> <value>   a given cell
#   gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)
#
# All coordinates are 0-based (top-left cell is 0 0).

gt 0 1 0 0
gt 0 2 0 1
gt 1 0 1 1
gt 2 3 2 4
gt 3 0 4 0
gt 3 4 3 3
gt 4 4 4 3
gt 1 3 2 3
