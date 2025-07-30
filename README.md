# wfey
explorations in waiting for events


# How Savepoint Works

## Checkout Branch to Mess Around in
```
git checkout -b dont-do-this-at-home
```
### Make sure branch is aligned with main if not already
```
git reset --hard origin/main
```

## Move to the last commit that was saved
```
git reset --hard \<commit hash\>
```

## Move to the last commit you want to include in the savepoint
** This command sets the state of the index to be as it would just after a merge from that commit **
```
git merge --squash \<commit hash\>
```

## Create Squashed Commit
```
git commit
```

## Go Back to Savepoint Branch and Grab New Commit
```
git checkout savepoint
git cherry-pick \<new dont-do-this-at-home commit hash\>
git push -f
```