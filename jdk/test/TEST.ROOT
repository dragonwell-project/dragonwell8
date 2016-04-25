# This file identifies the root of the test-suite hierarchy.
# It also contains test-suite configuration information.

# The list of keywords supported in the entire test suite.  The
# "intermittent" keyword marks tests known to fail intermittently.
# The "randomness" keyword marks tests using randomness with test
# cases differing from run to run. (A test using a fixed random seed
# would not count as "randomness" by this definition.) Extra care
# should be taken to handle test failures of intermittent or
# randomness tests.

keys=2d dnd i18n intermittent randomness

# Tests that must run in othervm mode
othervm.dirs=java/awt java/beans java/rmi javax/accessibility javax/imageio javax/sound javax/print javax/management com/sun/awt sun/awt sun/java2d sun/pisces sun/rmi

# Tests that cannot run concurrently
exclusiveAccess.dirs=java/rmi/Naming java/util/Currency java/util/prefs sun/management/jmxremote sun/tools/jstatd sun/security/mscapi javax/rmi

# Allow querying of sun.arch.data.model in @requires clauses
requires.properties=sun.arch.data.model 

# Group definitions
groups=TEST.groups [closed/TEST.groups]
