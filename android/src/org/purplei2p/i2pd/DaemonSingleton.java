package org.purplei2p.i2pd;

import java.util.HashSet;
import java.util.Set;

import android.util.Log;

public class DaemonSingleton {
	private static final String TAG="i2pd";
	private static final DaemonSingleton instance = new DaemonSingleton();
	public interface StateUpdateListener { void daemonStateUpdate(); }
	private final Set<StateUpdateListener> stateUpdateListeners = new HashSet<>();

	public static DaemonSingleton getInstance() {
		return instance;
	}

	public synchronized void addStateChangeListener(StateUpdateListener listener) { stateUpdateListeners.add(listener); }
	public synchronized void removeStateChangeListener(StateUpdateListener listener) { stateUpdateListeners.remove(listener); }

	private synchronized void setState(State newState) {
		if(newState==null)throw new NullPointerException();
		State oldState = state;
		if(oldState==null)throw new NullPointerException();
		if(oldState.equals(newState))return;
		state=newState;
		fireStateUpdate1();
	}
	public synchronized void stopAcceptingTunnels() {
		if(isStartedOkay()){
			setState(State.gracefulShutdownInProgress);
			I2PD_JNI.stopAcceptingTunnels();
		}
	}

	private volatile boolean startedOkay;

	public enum State {
        uninitialized(R.string.uninitialized),
        starting(R.string.starting),
        jniLibraryLoaded(R.string.jniLibraryLoaded),
        startedOkay(R.string.startedOkay),
        startFailed(R.string.startFailed),
        gracefulShutdownInProgress(R.string.gracefulShutdownInProgress),
        stopped(R.string.stopped);

        State(int statusStringResourceId) {
            this.statusStringResourceId = statusStringResourceId;
        }

        private final int statusStringResourceId;

        public int getStatusStringResourceId() {
            return statusStringResourceId;
        }
    };

	private volatile State state = State.uninitialized;

	public State getState() { return state; }

	{
		setState(State.starting);
		new Thread(new Runnable(){

			@Override
			public void run() {
				try {
					I2PD_JNI.loadLibraries();
					setState(State.jniLibraryLoaded);
				} catch (Throwable tr) {
					lastThrowable=tr;
					setState(State.startFailed);
					return;
				}
				try {
					synchronized (DaemonSingleton.this) {
						daemonStartResult = I2PD_JNI.startDaemon();
						if("ok".equals(daemonStartResult)){
							setState(State.startedOkay);
							setStartedOkay(true);
						}else setState(State.startFailed);
					}
				} catch (Throwable tr) {
					lastThrowable=tr;
					setState(State.startFailed);
					return;
				}
			}

		}, "i2pdDaemonStart").start();
	}
	private Throwable lastThrowable;
	private String daemonStartResult="N/A";

	private void fireStateUpdate1() {
		Log.i(TAG, "daemon state change: "+state);
		for(StateUpdateListener listener : stateUpdateListeners) {
			try {
				listener.daemonStateUpdate();
			} catch (Throwable tr) {
				Log.e(TAG, "exception in listener ignored", tr);
			}
		}
	}

	public Throwable getLastThrowable() {
		return lastThrowable;
	}

	public String getDaemonStartResult() {
		return daemonStartResult;
	}

	private final Object startedOkayLock = new Object();

	public boolean isStartedOkay() {
		synchronized (startedOkayLock) {
			return startedOkay;
		}
	}

	private void setStartedOkay(boolean startedOkay) {
		synchronized (startedOkayLock) {
			this.startedOkay = startedOkay;
		}
	}

	public synchronized void stopDaemon() {
		if(isStartedOkay()){
			try {I2PD_JNI.stopDaemon();}catch(Throwable tr){Log.e(TAG, "", tr);}
			setStartedOkay(false);
			setState(State.stopped);
		}
	}
}
