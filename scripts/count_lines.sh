find . -name "*.[chS]" -print >.files
find . -name Makefile -print >>.files
find . -name "*.proto" -print >>.files
find . -name "*.xml" -print >>.files
grep -v oss .files >.files1
grep -v "jobs.pb-c*" .files1 >.files2
grep -v "cli.pb-c*" .files2 >.files3
cat .files3 | xargs wc -l
rm -f .files .files1 .files2 .files3

