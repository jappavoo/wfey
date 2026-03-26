# wfey

explorations in waiting for events

## PreReqs

### Setting up Python Virtual Env

Create the virt env with all the required packages for the notebooks
```console
python -m venv venv
source /venv/bin/activate
pip install -r requirements.txt
```
> If you install a new required package you can update
requirements.txt using  
> `pip freeze > requirements.txt`

### PDU

You are expected to have:
- A file `wfey/scripts/pdu/pw.sh` that contains your password in plain
  text
- `expect` package to be able to code to the pdu

## How This Works

### Makefile

5 different configurations
- busypoll w/ no doorbell
- busypoll w/ doorbell
- wfe w/ no doorbell
- wfe w/ doorbell & no monitor
- wfe w/ doorbell & uses global monitor

### wfey

`./<wfey config> <event rate> <num event processors> <num sources>`

*event rate* is number of events per source  
*event processors* wait on events either with busypoll or wfe  
*sources* create events at a random interval depending on the event
rate and macro `DELAYADD`
- *DELAYADD* is the max percentage of variance to the delay between
  events
  
> [!NOTE] 
> The number of event processors is currently fixed at 1  
>> *I believe we can start to increase this with no side-effects but I
  haven't tested that yet* 

---

|        Macro        | Use                                                                                                          |
|:-------------------:|--------------------------------------------------------------------------------------------------------------|
|      WITHPERF       | Specifies level of perf gathering is done. <br> The higher the number[^1] the more intrusive the sampling is |
|  NULL\_WORK\_COUNT  | How long the work loop is                                                                                    |
|      DELAYADD       | The amount of % variance added/subtracted to the delay between events per source                             |
|      SECTORUN       | How long the benchmark runs the event processor for                                                          |
| TOTAL\_PERF\_EVENTS | How many PERF events are gathered -- Currently (2) CPU cycles and HW instructions                            |
|     SOCKETSPLIT     | Is defined if the cores span multiple sockets                                                                |
|       VERBOSE       | Defined for more indepth print statements                                                                    |

---

*TODO:*
- [ ] Change the way chosing cores are input from amount to bitmask
- [ ] Increase the number of event processors
- [ ] Implement mailbox
	- Similar to doorbell but a message and more information is able to
    be passed to the event processor
- [ ] Implement tripwire
	- This means that the memory address that triggers the event doesn't
	need to be known by the event source but the event processor must
    know when it is triggered anyways
- [ ] Make more work type scenarios instead of just null work


### wfey_hwmon

This file caches the path to the file that contains the core's energy
reading so it can be easily read from again later in the benchmark.

First call to `wfey_hwmon_joules_read` checks to see if the file path
to the *hwmon device* for this core has already been saved
- if it has, the file is opened and read from
- if it hasn't: the entire file path is dynamically found and cached
  - (all of this happens during initialization)

We use this function to grab the start and end energy in the event
processor

### scripts

`full_run.sh` -- runs a sweep of tests and organizes the output data  
Tells you:
- the environment variables used when running the test
- the loadavg on the machine (to check for possible computational
  interference) 
  
`env.sh` -- sets environment variables needed for infrastructure
  scripts 
- this is where you set which script is used to gather energy data
  - this is called in `run_wfey.sh`
  - this file should have:
	- which energy script is being used
	- the energy type[^2]
	- how much sleep time to pad at the end of a test for the energy
	numbers to reach their peak
	- how much sleep time to pad between tests to reset the energy
	numbers 
- `BM_ENERGY` is set when you want constant energy numbers to be
gathered during the execution of the benchmark[^3]

`run_wfey.sh` -- creates the proper data folder, runs the benchmark,
and gathers data as specified by `ENERGYSCRIPT`, `ENERGYTYPE`, and
`BM_ENERGY` 
- if `ENERGYTYPE` is `nomod`: the `ENERGYSCRIPT` will gather
  across-all-cores data periodically using the built-in hardware
  monitors[^4]
- if `ENERGYTYPE` is `wmod`: the `ENERGYSCRIPT` will gather gather per
  core data before and after the benchmark finishes executing[^5]
  - **NOTE:** Nothing assumes this mod is loaded

> PDU
>> ***WIP*** -- scripts to gather rack level energy numbers
>> functionally works but not integrated into the other scripts
>> would be fun sidequest if time allows


*TODO:*
- [ ] integrate pdu energy measuring scripts
- [ ] change *BM_ENERGY* to be flipped  
  - e.g. if the variable is defined that means we want energy to be gathered by the scriptsoutside the benchmark 

### notebooks

`SetUp.py`:
- *LOGS* and *DF* specify which logs directory is being processed and
  where the data frames should be exported
- The entire suite of benchmark configurations, event rates,
  event proc cpus, and source cpus are specified here but can be
  narrowed down later when graphing plots in `wfey_sweep`

`Process_Data.py`
- Goes through all configs and creates a dataframe for the outside bm
  data (if applicable), the bm data, and the latency data
- For every specific configuration, the mean is found *(TODO: confirm
  that the mean is being found correctly)*
- Pickles the dataframe for future use

`wfey_sweep.py`
- Subsets of the original suite of configs can chosen to graph
- 

> How to View Notebook diffs:  
>> Currently using
>> [nbdime](https://nbdime.readthedocs.io/en/latest/installing.html)  
>> To activate on notebooks `nbdime extensions --enable`


*TODO:*

- [ ] Deprecate the notebooks that don't use the hwmon module
- [ ] Understand what gather the middle data/not *RAW* means in `Process_Data`
- [ ] Confirm that the new way of getting data from the benchmark is being processed correctly
- [ ] Deprecate outlier removal for module or see if necessary
	  
### misc

`basic_tests` -- very simple test code that checks the energy spent on
one core over different combinations of *wfe*, *sleep*, and *busypoll*
with interleavings of *sev* in the background  
The goal of this script was to validate the energy differences between
*wfe*, *sleep*, and *busypoll* on the small scale
- *TODO:*
  - Create the ability to have more than one *sev* program in the
    background
  - Automate runner script to sweep
  - Create basic plotter for data

[^1]: WITHPERF Specifications  
	= 0 & no performance values  
	= 1 & only PERF events  
	= 2 & PERF events and acitive/inactive timing
	
[^2]: this dictate how cleanup occurs

[^3]: this itself might cause interference and data skew even if on
	its own separate core or it will not be a clear and tight
	representation of the energy expended in an event processor core
	
[^4]: data from `/sys/class/hwmon/hwmon<n>/power1_input` is the total
	energy output in $\mu$W -- therefore it is the rate of energy
	consumed at any given moment  

[^5]: data from `sensors altra_hwmon-isa-0000` is a per-core
	cumulative joules consumed -- therefore we can get the total
	energy consumed on specific cores throughout the course of a run
