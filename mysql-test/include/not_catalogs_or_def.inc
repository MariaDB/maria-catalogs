# Check that we are not using catalogs or we are using the 'def' catalog
if (`select @@catalogs = 1`)
{
  if (`select catalog() <> "def"`)
  {
    --skip This test works only without catalogs or when using the 'def' catalog
  }
}
