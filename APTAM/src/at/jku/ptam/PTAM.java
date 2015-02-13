//Copyright 2015 ICGJKU
package at.jku.ptam;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
//import javax.microedition.khronos.opengles.GL11;

import com.android.texample.GLText;
import at.jku.ptam.R;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SubMenu;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.Window;
import android.view.WindowManager;
import android.os.Bundle;
import android.opengl.GLES20;
import android.opengl.GLES10;
import android.opengl.GLSurfaceView;
import android.preference.PreferenceManager;
import at.jku.ptam.CameraManager.FrameListener;

public class PTAM extends Activity implements GLSurfaceView.Renderer,OnTouchListener,FrameListener{
	private CameraManager cameraManager;
	private static VideoSource vs;
	
	private GLSurfaceView glSurfaceView;
	private static GLText glText;
	private static String fdir;
	private boolean pauserender = false;
	
	private boolean requestExit = false;
	
	SharedPreferences preferences;
	
	static {
		System.loadLibrary("gnustl_shared"); 
		System.loadLibrary("PTAM"); 
	}
	
	public static VideoSource getVideoSource()
	{
		return vs;
	}
	
	public static String getFDir()
	{
		return fdir;
	}
	
	/*@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    // Inflate the menu items for use in the action bar
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.mainmenu, menu);
	    return super.onCreateOptionsMenu(menu);
	}*/
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
	                                ContextMenuInfo menuInfo) {
	    super.onCreateContextMenu(menu, v, menuInfo);
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.mainmenu, menu);
	    boolean docalibration = preferences.getBoolean("docalibration", false);
	    if(docalibration)
	    	menu.findItem(R.id.docalibration).setTitle("Disable Calibration");
	    SubMenu sm = menu.addSubMenu("White Balance Mode");
	    
	    MenuItem checkedmi = null;
	    String selectedmode = preferences.getString("wbmode", "auto");
	    for(String s: cameraManager.wbmodes)
	    {
	    	MenuItem mi = sm.add(1, Menu.NONE, Menu.NONE, s);
	    	mi.setCheckable(true);
	    	if(s.equalsIgnoreCase(selectedmode))
	    		checkedmi = mi;
	    }
	    sm.setGroupCheckable(1,true,true);
	    if(checkedmi != null)
	    	checkedmi.setChecked(true);
	    
	    boolean dolock = preferences.getBoolean("exposurelock", true);
	    if(!dolock)
	    	menu.findItem(R.id.dolock).setTitle("Enable Exposure Lock");
	    
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
	    
	    switch (item.getItemId()) {
	        case R.id.docalibration:
	        	boolean docalibration = preferences.getBoolean("docalibration", false);
	        	docalibration = !docalibration;
	        	SharedPreferences.Editor ce = preferences.edit();
	        	ce.putBoolean("docalibration", docalibration);
	        	ce.commit();
	            return true;
	        case R.id.dolock:
	        	boolean dolock = preferences.getBoolean("exposurelock", true);
	        	dolock = !dolock;
	        	SharedPreferences.Editor ce3 = preferences.edit();
	        	ce3.putBoolean("exposurelock", dolock);
	        	ce3.commit();
	            return true;
	        default:
	        	if(item.getGroupId()==1)
	        	{
	        		item.setChecked(true);
	        		String wbmode = item.getTitle().toString();
	        		Log.d("menu wb selected", wbmode);
	        		SharedPreferences.Editor ce2 = preferences.edit();
		        	ce2.putString("wbmode", wbmode);
		        	ce2.commit();
		        	cameraManager.UpdateWhiteBalanceMode();
	        	}
	            return super.onContextItemSelected(item);
	    }
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		
		preferences = PreferenceManager.getDefaultSharedPreferences(this);
		cameraManager = new CameraManager(preferences);
		
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD |
			    WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
			    WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
			    WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		
		this.requestWindowFeature(Window.FEATURE_NO_TITLE);
		
		
		cameraManager.startCamera(this);
		
		vs = new VideoSource(cameraManager);
		
		if(!this.getExternalFilesDir(null).exists())
		{
			this.getExternalFilesDir(null).mkdirs();
		}
		fdir = this.getExternalFilesDir(null).getAbsolutePath();
		
		Log.d("fdir", fdir);
		
		copyAssets();
		
		
		glText = null;
		
		boolean docalibration = preferences.getBoolean("docalibration", false);
		nativeInit(docalibration);
		
		glSurfaceView = new GLSurfaceView(this);
		glSurfaceView.setEGLContextClientVersion(2);
		glSurfaceView.setEGLConfigChooser(8, 8, 8, 8, 16, 0);
		glSurfaceView.setPreserveEGLContextOnPause(true);
		glSurfaceView.setRenderer(this);
		glSurfaceView.setOnTouchListener(this);
		glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
		//mGLView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
		setContentView(glSurfaceView);
		
		registerForContextMenu(glSurfaceView);
		
		cameraManager.setFrameListener(this);
	}
	
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
		if(keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_FOCUS)
		{
			nativeKey(32);//space
			return true;
		}
		if(keyCode ==KeyEvent.KEYCODE_CAMERA)
		{
			cameraManager.FixCameraSettings();
		}
		if(keyCode == KeyEvent.KEYCODE_VOLUME_UP)
		{
			nativeKey(32);
			return true;
		}
		if(keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
		{
			cameraManager.FixCameraSettings();
			//nativeKey(13);//enter
			return true;
		}
		return super.onKeyDown(keyCode, event);
	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
	    super.onConfigurationChanged(newConfig);
	}

	@Override
	public void onResume() {
		super.onResume();
		glSurfaceView.onResume();
	}

	@Override
	public void onPause() {
		super.onPause();
		glSurfaceView.onPause();
	}

	@Override
	public void onStop() {
		cameraManager.stopCamera();
		nativeDestroy();
		super.onStop();
		System.exit(0);//hack
	}

	@Override
	public void onDestroy() {
		super.onDestroy();
	}
	
	@Override
	public void onBackPressed() {
	    new AlertDialog.Builder(this)
	        .setTitle("Really Exit?")
	        .setMessage("Are you sure you want to exit?")
	        .setNegativeButton(android.R.string.no, null)
	        .setPositiveButton(android.R.string.yes, new OnClickListener() {

	            public void onClick(DialogInterface arg0, int arg1) {
	            	requestExit = true;
	            }
	        }).create().show();
	}
	
	@Override
	public boolean onTouch(View v, MotionEvent event) {

		if(event.getAction()==MotionEvent.ACTION_DOWN)
		{
			int x = (int)event.getX();
			int y = (int)event.getY();
			nativeClick(x,y);
			return false;
		}
		return false;
	}

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {
	
	  cameraManager.createRenderTexture();
		
		// Create the GLText
      glText = new GLText( this.getAssets() );

      // Load the font from file (set size + padding), creates the texture
      // NOTE: after a successful call to this the font is ready for rendering!
      glText.load( "Roboto-Regular.ttf", 14, 2, 2 );  // Create Font (Height: 14 Pixels / X+Y Padding 2 Pixels)
      
      nativeInitGL();
		
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height) {
		//gl.glViewport(0, 0, width, height);
		
		// Setup orthographic projection
	      /*gl.glMatrixMode( GL10.GL_PROJECTION );  
	      gl.glLoadIdentity();                      
	      gl.glOrthof(                              
	         0, width,
	         0, height,
	         -1.0f, 1.0f
	      );*/
	      
	    nativeResize(width, height);
	}

	@Override
	public void onDrawFrame(GL10 gl) {
		if(!pauserender)
		{
			android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO); //probably does not help
			
			//wait for new camera frame
			int count = 0;
			while(!cameraManager.isCameraImageReady() && count < 100)
			{
				try {
					count++;
					Thread.sleep(5);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
			if(count==100)
				Log.e("Timeout","Timeout while waiting for next camera image!");
			if(cameraManager.isCameraImageReady())
			{
				GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				GLES20.glClear(GLES10.GL_COLOR_BUFFER_BIT|GLES10.GL_DEPTH_BUFFER_BIT);
				//gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				//gl.glClear(GLES10.GL_COLOR_BUFFER_BIT|GLES10.GL_DEPTH_BUFFER_BIT);
				if(requestExit)
				{
					if(nativeFinish())
					{
						//wait some ms
						try {
							Thread.sleep(100);
						} catch (InterruptedException e) {
							e.printStackTrace();
						}
						if(nativeFinish())
							finish();
					}
				}
				else
					nativeRender();
			}
		      /*gl.glMatrixMode( GL10.GL_MODELVIEW );      
		      gl.glLoadIdentity();*/      
			//gl.glColor4f(1, 0, 0, 1);
			//drawText("test",100,10);
		}
	}
	
	public static void drawText(String text, int x, int y, int shaderid)
	{
		//This is a quick and dirty hack to get text rendering support to Opengl ES 2.0, shader is defined in cpp part, ...
		if(glText!=null)
		{
			glText.EnableGLSettings();
			
			glText.begin(shaderid);        
	        glText.draw( text, x, y ); 
	        glText.end();
	        
	        glText.DisableGLSettings();
		}
	}
	
	/*
	 * A native method that is implemented by the 'hello-ptam' native library,
	 * which is packaged with this application.
	 */
	private native void nativeInit(boolean docalibration);
	private native void nativeDestroy();
	private native void nativeInitGL();
	private native void nativeResize(int w, int h);
    private native void nativeRender();
    private native boolean nativeFinish();
    //private static native void nativeDone();
    private native void nativeClick(int x, int y);
    private native void nativeKey(int keycode);

	@Override
	public void onFrameReady() {
		glSurfaceView.requestRender();
	}
	
	private void copyAssets() {
	    AssetManager assetManager = getAssets();
	    String[] files = null;
	    try {
	        files = assetManager.list("");
	    } catch (IOException e) {
	        Log.e("tag", "Failed to get asset file list.", e);
	    }
	    for(String filename : files) {
	    	if(!filename.endsWith(".cfg"))//hack to skip non cfg files
	    		continue;
	        InputStream in = null;
	        OutputStream out = null;
	        try {
	          in = assetManager.open(filename);
	          File outFile = new File(fdir, filename);
	          if(outFile.exists())
	          {
	        	  Log.d("copy assets", "File exists: " + filename);
	          }
	          else
	          {
	        	  out = new FileOutputStream(outFile);
	        	  copyFile(in, out);
	        	  Log.d("copy assets", "File copied: " + filename);
	          }
	        } catch(IOException e) {
	            Log.e("tag", "Failed to copy asset file: " + filename, e);
	        }     
	        finally {
	            if (in != null) {
	                try {
	                    in.close();
	                } catch (IOException e) {
	                    // NOOP
	                }
	            }
	            if (out != null) {
	                try {
	                    out.close();
	                } catch (IOException e) {
	                    // NOOP
	                }
	            }
	        }  
	    }
	}
	
	private void copyFile(InputStream in, OutputStream out) throws IOException {
	    byte[] buffer = new byte[1024];
	    int read;
	    while((read = in.read(buffer)) != -1){
	      out.write(buffer, 0, read);
	    }
	}

}
