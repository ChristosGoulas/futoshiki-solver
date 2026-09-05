# Example 5x5 Futoshiki puzzle.
#
# Format (one directive per line, '#' starts a comment):
#   fixed <row> <col> <value>   a given cell
#   gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)
#
# All coordinates are 0-based (top-left cell is 0 0).

fixed 0 4 5

gt 0 4 0 3
gt 1 0 2 0
