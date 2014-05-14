echo "Commits by authors"
git log --pretty='%aN <%aE>'  | sort | uniq -c | sort -rn | nl
