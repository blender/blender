Snap Configuration
===================

Files used by Buildbot's `package-code-store-snap` and `deliver-code-store-snap` steps.

Build pipeline snap tracks and channels

```
    <track>/stable            
        - Latest stable release for the specified track
    <track>/candidate         
        - Test builds for the upcoming stable release - *not used for now*
    <track>/beta              
        - Nightly automated builds provided by a release branch
    <track>/edge/<branch>
        - Nightly or on demand builds - will also make use of branch
```
