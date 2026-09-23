"""Group value of a single row: greedy group forming vs. closed formulas.

A row is described by the number of tiles per color. A group is a set of at
least s tiles with different colors. The group value is the maximum number of
tiles that can be placed in groups (thesis, Sections 4.4 and 4.5).

Without arguments, all configurations with k colors and at most m copies are
generated, and the closed formulas are checked against the greedy algorithm
with asserts. With arguments, the groups for one configuration are printed:

  python scripts/group_value.py            # check all configurations (k=6, m=6, s=3)
  python scripts/group_value.py 3 4 6 7    # groups for 3, 4, 6 and 7 tiles per color
"""

from itertools import combinations_with_replacement
import sys


def max_groups(config, s=3):
    """Calculates the maximum group value for a given configuration of tiles and minimal group size s.
       Args:
        config: A dictionary where keys are tile colors and values are the number of tiles of that color.
        s: The minimum size of a group.
       Returns:
        groups: A list of formed groups to get the group value.
        A dictionary with remaining tile colors and their counts after forming groups."""

    # sort the configuration by value and discard any zero values
    copies = sorted([value, key] for key, value in config.items() if value > 0)

    # groups is a list of formed groups
    groups = []

    # as long there are more colors with tiles than the minimal set size
    while len(copies) >= s:
        while copies[0][0] > 0:
            # form a group with the first color
            copies[0][0] -= 1
            group = [copies[0][1]]

            # add the last s-1 colors to the group
            for idx in range(len(copies) - s + 1, len(copies)):
                copies[idx][0] -= 1
                group.append(copies[idx][1])
            groups.append(sorted(group))

            copies.sort()

        # remove any colors that are now empty
        copies = [[value, key] for value, key in copies if value > 0]

    # if there are still tiles left we can try to put them in groups which don't contain this color yet
    for idx, (value, key) in enumerate(copies):
        for _ in range(value):
            for group in groups:
                if key not in group:
                    copies[idx][0] -= 1
                    group.append(key)
                    group.sort()
                    break

    return sorted(groups), {key: value for value, key in copies if value > 0}

def form_groups(config, k, s=3):
    """Form the groups of tiles according to the closed formula for fixed minimal group size s.
       Args:
        config: A dictionary where keys are tile colors and values are the number of tiles of that color.
        k: The number of colors.
        s: The minimum size of a group.
       Returns:
        groups: A list of formed groups to get the group value.
        A dictionary with remaining tile colors and their counts after forming groups."""

    #list of formed groups
    groups = []

    # if there are less colors that minimal group size we can return the empty list
    # since no groups can be formed
    if k < s:
        return groups, config

    # sort the configuration and discard any zero values
    copies = sorted([value, key] for key, value in config.items() if value > 0)

    # tiles is the list of tiles without colors
    tiles = sorted(config.values())

    #take the sum of the first k-2 colors
    sum_tiles = sum(tiles[:k - 2])

    # if the sum is less than or equal to the number of tiles of the k-2 color (second largest color)
    # make groups of size s = 3
    if (sum_tiles <= tiles[k - 2]):
        # as long there are tiles in the first k -2 colors
        while len(copies) >= s:
            while copies[0][0] > 0:
                # form a group with the first color
                copies[0][0] -= 1
                group = [copies[0][1]]

                # add the last s-1 = 2 colors to the group
                for idx in range(len(copies) - s + 1, len(copies)):
                    copies[idx][0] -= 1
                    group.append(copies[idx][1])
                # add the group to the list of groups
                groups.append(sorted(group))

                copies.sort()

            # remove any colors that are now empty
            copies = [[value, key] for value, key in copies if value > 0]

        return sorted(groups), {key: value for value, key in copies if value > 0}

    # If the sum is indeed bigger we can add the number of tiles of the k-2 color to the sum
    sum_tiles += tiles[k - 2]

    #number of groups is minimum of value in k and the sum of the tiles divided by 2
    number_of_groups = min(tiles[k - 1], sum_tiles // 2)

    # smallest group size (-1 since the last color is always added) of the formed groups
    smallest_group_size = sum_tiles // number_of_groups

    # Number of groups with smallest size + 1 (the number of groups that has a tile more than the others)
    plus1_groups = sum_tiles % number_of_groups

    # as long there are tiles in the last color we form groups
    for i in range(number_of_groups):
        group = []

        # add the first color to the group
        copies[0][0] -= 1
        group.append(copies[0][1])

        # first plus1_groups get another tile too
        # which is the last color - smallest_group_size - 1
        if (i < plus1_groups):
            copies[len(copies)- smallest_group_size - 1][0] -= 1
            group.append(copies[len(copies)- smallest_group_size - 1][1])

        # add the last smallest_group_size colors to the group
        for idx in range(len(copies) - smallest_group_size, len(copies)):
            copies[idx][0] -= 1
            group.append(copies[idx][1])

        # add the formed group to the list of groups
        groups.append(sorted(group))

        copies.sort()

        # remove any colors that are now empty
        copies = [[value, key] for value, key in copies if value > 0]

    return sorted(groups), {key: value for value, key in copies if value > 0}

def rem_s3(config, k):
    """Calculates the remaining value of a configuration of tiles after forming groups of minimal size 3.
       Args:
        config: A dictionary where keys are tile colors and values are the number of tiles of that color.
        k: The number of colors.
       Returns:
        The remaining number of tiles of the configuration after forming groups to get the max value."""

    # if the number of colors is less than the minimal set size we can return the number of tiles
    if k < 3:
        return sum(config.values())

    # sort the configuration by value
    copies = sorted(config.values())

    # take the sum of the first k-2 colors
    t = sum(copies[:k - 2])

    # if the sum is greater than the number of tiles of the k-2 color (second largest color)
    if t > copies[k - 2]:
        return max(0, copies[k - 1] - (t + copies[k - 2]) // 2)

    return copies[k - 1] + copies[k - 2] - 2 * t


def groupvalue_fixed_s(config, k):
    """Calculates the group value for a given configuration of tiles and fixed group size 3 using a closed formula.
       Args:
        config: A dictionary where keys are tile colors and values are the number of tiles of that color.
        k: The number of colors.

       Returns:
        The group value for the configuration."""

    # if the number of colors is less than the minimal set size we can't make any groups
    if k < 3:
        return 0

    # sort the configuration by value
    copies = sorted(config.values())

    # take the sum of the first k-2 colors
    sum_tiles = sum(copies[:k - 2])

    # if the sum is less than or equal to the number of tiles of the k-2 color (second largest color)
    if (sum_tiles <= copies[k - 2]):
        return sum_tiles * 3

    # otherwise add the number of tiles of the k-2 color to the sum
    sum_tiles += copies[k - 2]

    # add the largest possible value of the last color and return
    return sum_tiles + min(copies[k - 1], sum_tiles // 2)


def groupvalue_arbitrary_s(config, k, s=3):
    """Calculates the group value for a given configuration of tiles and arbitrary group size s using a closed formula.
       Args:
        config: A dictionary where keys are tile colors and values are the number of tiles of that color.
        k: The number of colors.
        s: The minimum size of a group.
       Returns:
        The group value for the configuration."""

    # if the number of colors is less than the minimal set size we can't make any groups
    if k < s:
        return 0

    # sort the configuration by value
    copies = sorted(config.values())

    # take the sum of the first k-(s-1) colors
    sum_tiles = sum(copies[:k - (s-1)])

    # if the sum is less than or equal to the number of tiles of the k-(s-1) color
    if (sum_tiles <= copies[k - (s-1)]):

        # return the sum multiplied by the size of the group
        return sum_tiles * s

    # otherwise add the number of tiles of the k-(s-1) color to the sum
    sum_tiles += copies[k - (s-1)]

    # we divide the sum of tiles by the divider which starts at 2 and increases by 1 for each color added
    divider = 2

    # for each remaining colors
    for i in range(k-(s-2), k):
        # add the optimal number of tiles to the sum
        sum_tiles+= min(copies[i], sum_tiles// divider)
        divider += 1

    #after adding all colors we return the sum
    return sum_tiles

def main():


    # input parameters
    s = 3
    k = 6
    m = 6

    # if the user provided a configuration as command line arguments
    if len(sys.argv) > 1:
        # initialize the configuration with the provided values
        config = {chr(ord("a") + idx): int(arg) for idx, arg in enumerate(sys.argv[1:])}

        # make optimal groups with the provided configuration
        groups, remainder = max_groups(config,s)

        # print the formed groups
        for group in groups:
            print(group)

        total_value = sum(config.values())
        max_value = sum(len(group) for group in groups)
        rem_value = sum(remainder.values())
        print(total_value, max_value, rem_value, remainder)

        return

    # if no configuration was provided, generate all possible configurations for the given parameters
    colors = "".join(chr(ord("a") + idx) for idx in range(k))
    for combo in combinations_with_replacement(range(m + 1), k):

        # create a configuration dictionary with the generated values
        config = {key: value for key, value in zip(colors, combo)}

        # if s = 3, also check the groups formed by the closed formula for s = 3
        if (s == 3):
            groups, remainder = form_groups(config, k, 3)
            rem_value = sum(remainder.values())
            rem = rem_s3(config, k)
            assert rem == rem_value
        else:
            groups, remainder = max_groups(config, s)

        total_value = sum(config.values())

        # groupvalue according to the greedy algorithm
        max_value = sum(len(group) for group in groups)

        #group value according to the closed formula for arbitrary s
        group_value = groupvalue_arbitrary_s(config, k, s)

        print(config, total_value, max_value, group_value)
        print(groups, remainder)
        print()
        assert group_value == max_value

if __name__ == "__main__":
    main()

