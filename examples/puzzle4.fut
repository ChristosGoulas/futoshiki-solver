# Example 4x4 Futoshiki puzzle -- requires --size 4 (the default is 5x5).
#
# Format (one directive per line, '#' starts a comment):
#   fixed <row> <col> <value>   a given cell
#   gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)
#
# All coordinates are 0-based (top-left cell is 0 0).
#
# Run with: ./futoshiki --size 4 examples/puzzle4.fut

fixed 0 0 1
fixed 2 3 2

gt 0 3 0 0
gt 1 1 1 0
gt 2 1 2 2
gt 3 0 3 1
