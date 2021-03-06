subroutine da_read_obs_radar (iv, filename, grid)

   !-----------------------------------------------------------------------
   ! Purpose: Read the radar observation file
   !----------------------------------------------------------------------------------------!

   implicit none

   type (iv_type),    intent(inout) :: iv
   character(len=*),  intent(in)    :: filename
   type(domain),     intent(in)     :: grid     ! first guess state.

   integer                       :: i, j, n, nn, iost, nlevels, fm
   integer                       :: total_radar
   integer                       :: iunit

   type (radar_multi_level_type) :: platform

   character (LEN = 120)         :: char_total_radar
   character (LEN = 120)         :: char_ned

   logical                       :: outside, outside_all
   integer                       :: n_dup, ndup

   real*8                        :: obs_time, analysis_time
   integer                       :: iyear, imonth, iday, ihour, imin
   real                          :: tdiff, dlat_earth,dlon_earth,crit
   integer                       :: itt,itx,iout
   logical                       :: iuse, thin_3d, found
   integer                       :: i1,j1,k
   real                          :: dx,dy,dxm,dym,zk
   real                          :: v_h(kms:kme)

   integer                       :: ntotal,nlocal,ilocal
   integer                       :: radar_nlocal
   real, allocatable             :: in(:), out(:)


   if (trace_use) call da_trace_entry("da_read_obs_radar")

   thin_3d=.false.
   if( thin_conv_ascii) thin_3d=.true.
   ntotal = iv%info(radar)%ptotal(iv%time-1)
   nlocal = iv%info(radar)%plocal(iv%time-1)
   ilocal = nlocal 
   radar_nlocal = nlocal
 
   ! 1. open file

   call da_get_unit(iunit)
   open(unit   = iunit,     &
        FILE   = trim(filename), &
        FORM   = 'FORMATTED',  &
        ACCESS = 'SEQUENTIAL', &
        iostat =  iost,     &
        STATUS = 'OLD')

   if (iost /= 0) then
      ! Missing file does not matter
      call da_warning(__FILE__,__LINE__, &
         (/"Cannot open radar file "//filename/))
      call da_free_unit(iunit) 
      if (trace_use) call da_trace_exit("da_read_obs_radar")
      return
   end if

   ! 2. read total radar

   !  2.1 read first line

   read (unit=iunit, fmt = '(A)', iostat = iost) char_total_radar

   !  2.2 process error

   if (iost /= 0) then
     call da_error(__FILE__,__LINE__, &
        (/"Cannot read radar file"/))
   end if

   !  2.3 total radar number

   read (unit=char_total_radar (15:17),fmt='(I3)', iostat = iost) total_radar

   if (print_detail_radar) write (unit=stdout,fmt='(/,A,I3,/)') &
       ' TOTAL RADAR: ', total_radar

   !  2.4 skip one line

   read (unit=iunit, fmt = '(A)', iostat = iost)

   ! 3. read radar data

   do nn = 1, total_radar
 
   if ( thin_conv_ascii ) call cleangrids_conv(radar)
      ! 3.1 skip one blank line

      read (unit=iunit, fmt = '(A)', iostat = iost)

      ! 3.2 read header

      read (unit=iunit, fmt = '(A)', iostat = iost) char_ned

      ! 3.3 read header information

      read (unit=char_ned (1:5),fmt='(A5)', iostat = iost) platform % stn % platform

      if (print_detail_radar) write (unit=stdout,fmt='(A)') 'RADAR Observations information'

      read (unit=char_ned (8:19),fmt='(A12)', iostat = iost) platform % stn % name

      if (print_detail_radar) write (unit=stdout,fmt='(A,A5,A,A12)')  &
                           ' Expect: ',platform % stn % platform, &
                           'data at station:', platform % stn % name

      read (unit=char_ned(20:27),fmt='(F8.3)', iostat = iost) platform % stn % lon

      read (unit=char_ned (30:37),fmt='(F8.3)', iostat = iost) platform % stn % lat

      read (unit=char_ned (40:47),fmt='(F8.1)', iostat = iost) platform % stn % elv

      if (print_detail_radar) write (unit=stdout,fmt='(A,2(F8.3,2X),F8.1)')  &
         'The station longitude, latutude, and altitude are: ', &
         platform % stn % lon, &
         platform % stn % lat, platform % stn % elv

      read (unit=char_ned (50:68),fmt='(A19)', iostat = iost) platform % stn % date_char

      if (print_detail_radar) write (unit=stdout,fmt='(A,A19)')   &
         'The observation time for this data is ',     &
         platform % stn % date_char

      read (unit=char_ned (69:74),fmt='(I6)', iostat = iost) platform % stn % numobs

      if (print_detail_radar) write (unit=stdout,fmt='(A,I6)')   &
         'Total number of Super-observations is ', &
         platform % stn % numobs


      read (unit=char_ned (75:80),fmt='(I6)', iostat = iost) platform % stn % levels

      if (print_detail_radar) write (unit=stdout,fmt='(A,I6)')   &
         'Vertical layers for each Super-observation is ', &
         platform % stn % levels

      ! 3.4 skip two lines

      read (unit=iunit, fmt = '(A)', iostat = iost)
      read (unit=iunit, fmt = '(A)', iostat = iost)

      ! 3.5 loop over records

      reports: do j = 1, platform % stn % numobs

         ! 3.5.1 read station general info

         read (unit = iunit, iostat = iost, &
                      fmt = '(A12,3X,A19,2X,2(F12.3,2X),F8.1,2X,I6)') &
                      platform % info % platform,  &
                      platform % info % date_char, &
                      platform % info % lat,       &
                      platform % info % lon,       &
                      platform % info % elv,       &
                      platform % info % levels
      if (platform%info%lon == 180.0  ) platform%info%lon =-180.000
      ! Fix funny wind direction at Poles
      if (platform%info%lat < -89.9999 .or. platform%info%lat > 89.9999) then
         platform%info%lon = 0.0
      end if

         read(platform % info % platform (4:6), '(I3)') fm

         ! 3.5.2 read each level

         do i = 1, platform % info % levels
            ! height
            platform%each(i) = radar_each_level_type(missing_r, missing, -1.0,       &
               field_type(missing_r, missing, missing_r, missing, missing_r), & ! rv
               field_type(missing_r, missing, missing_r, missing, missing_r))   ! rf

            read (unit = iunit, fmt = '(3X, F12.1, 2(F12.3,I4,F12.3,2X))') &
                             platform % each (i) % height,           &
                             platform % each (i) % rv % inv,         &
                             platform % each (i) % rv % qc,          &
                             platform % each (i) % rv % error,       &
                             platform % each (i) % rf % inv,         &
                             platform % each (i) % rf % qc,          &
                             platform % each (i) % rf % error

            if (platform % each (i) % rv % error == 0.0) then
                 platform % each (i) % rv % error  = 1.0
            end if

            if (platform % each (i) % rf % error == 0.0) then
                 platform % each (i) % rf % error  = 1.0
            end if

            if (platform % each (i) % rv % inv   == missing_r .or. &
                platform % each (i) % rv % error == missing_r) then
                platform % each (i) % rv % qc     = missing_data
            end if

            if (platform % each (i) % rf % inv   == missing_r .or. &
                platform % each (i) % rf % error == missing_r) then
                platform % each (i) % rf % qc     = missing_data
            end if
         end do

         ! Check if outside of the time range:

         read (platform%info%date_char,'(i4,4(1x,i2))') &
               iyear, imonth, iday, ihour, imin
         call da_get_julian_time (iyear,imonth,iday,ihour,imin,obs_time)
         if ( obs_time < time_slots(0) .or. &
              obs_time >= time_slots(num_fgat_time) ) then
            if (print_detail_radar) then
               write(unit=stdout, fmt='(a)') '*** Outside of the time range:'
               write(unit=stdout, fmt=fmt_info) &
                     platform%info%platform,    &
                     platform%info%date_char,   &
                     platform%stn%name
            end if
            cycle reports
         endif

         call da_llxy (platform%info, platform%loc, outside, outside_all)
         if( outside_all ) then
            if (print_detail_radar) then
               write(unit=stdout, fmt='(a)') '*** Report is outside of domain:'
               write(unit=stdout, fmt=fmt_info) &
                     platform%info%platform,    &
                     platform%info%lat,   &
                     platform%info%lon,   &
                     platform%stn%name
            end if
            cycle reports
         end if

        read (analysis_date,'(i4,4(1x,i2))') &
                                    iyear, imonth, iday, ihour, imin
        call da_get_julian_time (iyear,imonth,iday,ihour,imin,analysis_time)
        tdiff = abs((obs_time - analysis_time)-0.02)
        dlat_earth = platform%info%lat
        dlon_earth = platform%info%lon
        if (dlon_earth < 0.0) dlon_earth = dlon_earth + 360.0
        if (dlon_earth >= 360.0) dlon_earth = dlon_earth - 360.0
        dlat_earth = dlat_earth * deg2rad
        dlon_earth = dlon_earth * deg2rad

         nlevels = platform%info%levels

            if( .not. thin_conv_ascii) then
            iv%info(radar)%max_lev = max(iv%info(radar)%max_lev, platform%info%levels)
            else
            iv%info(radar)%max_lev = 1
            end if

         if (nlevels > max_ob_levels) then
            write(unit=message(1),fmt='(A,2I8)') &
               ' radar=> nlevels > max_ob_levels:',nlevels, max_ob_levels
            call da_warning(__FILE__,__LINE__,message(1:1)) 
            nlevels = max_ob_levels
             platform%info%levels = nlevels
         else if (nlevels < 1) then
            cycle reports
         end if

         ! Loop over duplicating obs for global
         n_dup = 1
         if (global .and. &
            (platform%loc%i == ids .or. platform%loc%i == ide)) n_dup= 2
         do ndup = 1, n_dup
            select case (fm)
            case (128)
             if (.not.use_radarobs .or. ntotal == max_radar_input) cycle reports

            if( .not. thin_conv_ascii) then
             if (ndup==1 ) ntotal = ntotal + 1
             if (outside) cycle reports
                   nlocal = nlocal + 1
                   ilocal = ilocal + 1

               iv % radar (ilocal) % stn_loc % lon = platform % stn % lon
               iv % radar (ilocal) % stn_loc % lat = platform % stn % lat
               iv % radar (ilocal) % stn_loc % elv = platform % stn % elv

               iv%info(radar)%levels(ilocal)    = nlevels
               iv%info(radar)%name(ilocal)      = platform%info%name
               iv%info(radar)%platform(ilocal)  = platform%info%platform
               iv%info(radar)%id(ilocal)        = platform%info%id
               iv%info(radar)%date_char(ilocal) = platform%info%date_char
               iv%info(radar)%lat(:,ilocal)     = platform%info%lat
               iv%info(radar)%lon(:,ilocal)     = platform%info%lon
               iv%info(radar)%elv(ilocal)       = platform%info%elv
               iv%info(radar)%pstar(ilocal)     = platform%info%pstar
               iv%info(radar)%slp(ilocal)           = platform%loc%slp
               iv%info(radar)%pw(ilocal)            = platform%loc%pw
               iv%info(radar)%x(:,ilocal)           = platform%loc%x
               iv%info(radar)%y(:,ilocal)           = platform%loc%y
               iv%info(radar)%i(:,ilocal)           = platform%loc%i
               iv%info(radar)%j(:,ilocal)           = platform%loc%j
               iv%info(radar)%dx(:,ilocal)          = platform%loc%dx
               iv%info(radar)%dxm(:,ilocal)         = platform%loc%dxm
               iv%info(radar)%dy(:,ilocal)          = platform%loc%dy
               iv%info(radar)%dym(:,ilocal)         = platform%loc%dym
               iv%info(radar)%proc_domain(:,ilocal) = platform%loc%proc_domain
               iv%info(radar)%obs_global_index(ilocal) = ntotal

              if( nlocal == ilocal) then
               allocate (iv % radar (ilocal) % model_p  (1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % model_rho(1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % model_qrn(1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % height   (1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % height_qc(1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % rv       (1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % rf       (1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % rr       (1:iv%info(radar)%max_lev))
               allocate (iv % radar (ilocal) % rro      (1:iv%info(radar)%max_lev))
              end if
               do i = 1, nlevels
                  iv % radar (ilocal) % height(i)    = platform % each(i) % height
                  iv % radar (ilocal) % height_qc(i) = platform % each(i) % height_qc
                  iv % radar (ilocal) % rv(i)        = platform % each(i) % rv
                  iv % radar (ilocal) % rf(i)        = platform % each(i) % rf
               end do

            else ! if thin_conv_ascii==true
              thin_3d=.true.
              i1   = platform%loc%i
              j1   = platform%loc%j
              dx   = platform%loc%dx
              dy   = platform%loc%dy
              dxm  = platform%loc%dxm
              dym  = platform%loc%dym

                 do k=kms,kme
                  v_h(k) = dym*(dxm*grid%xb%h(i1,j1,k)+dx*grid%xb%h(i1+1,j1,k)) + &
                            dy*(dxm*grid%xb%h(i1,j1+1,k)+dx*grid%xb%h(i1+1,j1+1,k))
                 end do
              do k= 1, nlevels
             if (ndup==1 ) ntotal = ntotal + 1
             if (outside) cycle 
               zk = missing_r
                 call da_to_zk(platform%each(k)%height , v_h, v_interp_h, zk)
               if ( zk == missing_r ) cycle
               crit = tdiff
               call map2grids_conv(radar,dlat_earth,dlon_earth,crit,nlocal,itx,1,itt,ilocal,iuse,zk,thin_3d)
               if ( .not. iuse ) cycle
               iv%info(radar)%levels(ilocal)    = 1
               iv%info(radar)%name(ilocal)      = platform%info%name
               iv%info(radar)%platform(ilocal)  = platform%info%platform
               iv%info(radar)%id(ilocal)        = platform%info%id
               iv%info(radar)%date_char(ilocal) = platform%info%date_char
               iv%info(radar)%lat(:,ilocal)     = platform%info%lat
               iv%info(radar)%lon(:,ilocal)     = platform%info%lon
               iv%info(radar)%elv(ilocal)       = platform%info%elv
               iv%info(radar)%pstar(ilocal)     = platform%info%pstar

               iv%info(radar)%slp(ilocal)           = platform%loc%slp
               iv%info(radar)%pw(ilocal)            = platform%loc%pw
               iv%info(radar)%x(:,ilocal)           = platform%loc%x
               iv%info(radar)%y(:,ilocal)           = platform%loc%y
               iv%info(radar)%i(:,ilocal)           = platform%loc%i
               iv%info(radar)%j(:,ilocal)           = platform%loc%j
               iv%info(radar)%dx(:,ilocal)          = platform%loc%dx
               iv%info(radar)%dxm(:,ilocal)         = platform%loc%dxm
               iv%info(radar)%dy(:,ilocal)          = platform%loc%dy
               iv%info(radar)%dym(:,ilocal)         = platform%loc%dym
               iv%info(radar)%proc_domain(:,ilocal) = platform%loc%proc_domain
               iv%info(radar)%obs_global_index(ilocal) = ntotal 
               if( nlocal == ilocal) then
                allocate (iv % radar (ilocal) % model_p  (1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % model_rho(1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % model_qrn(1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % height   (1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % height_qc(1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % rv       (1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % rf       (1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % rr       (1:iv%info(radar)%max_lev))
                allocate (iv % radar (ilocal) % rro      (1:iv%info(radar)%max_lev))
               end if

               do i = 1, 1
                 iv % radar (ilocal) % height(i)    = platform % each(i) % height
                 iv % radar (ilocal) % height_qc(i) = platform % each(i) % height_qc
                 iv % radar (ilocal) % rv(i)        = platform % each(i) % rv
                 iv % radar (ilocal) % rf(i)        = platform % each(i) % rf
               end do
     
              end do ! loop over k levels

            end if ! if over thin_conv_ascii

            case default;
               write(unit=message(1), fmt='(a)') 'Unsaved obs found:'
               write(unit=message(2), fmt='(2a)') &
                  'platform % info % platform=', platform % info % platform
               write(unit=message(3), fmt='(a, i3)') &
                  'platform % info % levels=', platform % info % levels
               call da_warning(__FILE__,__LINE__,message(1:3))
            end select
            if (global .and. ndup == 1) then
               if (platform%loc % i >= ide) then
                  platform%loc%i = ids
                  platform%loc%proc_domain = .false.
               else if (platform%loc % i <= ids) then
                  platform%loc%i = ide
                  platform%loc%proc_domain = .false.
               end if
            end if
         end do        !  loop over duplicate
      end do reports
   ! thinning check
   if ( thin_conv_ascii ) then
      n = radar
          allocate ( in(thinning_grid_conv(n)%itxmax) )
          allocate (out(thinning_grid_conv(n)%itxmax) )
            do i = 1, thinning_grid_conv(n)%itxmax
               in(i) = thinning_grid_conv(n)%score_crit(i)
            end do
#ifdef DM_PARALLEL
            ! Get minimum crit and associated processor index.
            call mpi_reduce(in, out, thinning_grid_conv(n)%itxmax, true_mpi_real, mpi_min, root, comm, ierr)
            call wrf_dm_bcast_real (out, thinning_grid_conv(n)%itxmax)
#else
            out = in
#endif
            do i = 1, thinning_grid_conv(n)%itxmax
              if( out(i) < 9.99e6) iv%info(n)%thin_ntotal=  iv%info(n)%thin_ntotal + 1
               if ( abs(out(i)-thinning_grid_conv(n)%score_crit(i)) > 1.0E-10 ) then
                  thinning_grid_conv(n)%ibest_obs(i) = 0
               end if
            end do
            do j = radar_nlocal + 1 ,  nlocal        
               found = .false.
               do i = 1, thinning_grid_conv(n)%itxmax
                  if ( thinning_grid_conv(n)%ibest_obs(i) == j .and.         &
                       thinning_grid_conv(n)%score_crit(i) < 9.99e6 ) then
                   iv%info(n)%thin_nlocal =  iv%info(n)%thin_nlocal + 1
                     found = .true.

                     exit
                  end if
               end do
               if ( .not. found ) then
                  iv%info(n)%thinned(:,j) = .true.
               end if
            end do
         deallocate( in  )
         deallocate( out )
   end if  ! thin_conv_ascii

                radar_nlocal = nlocal             


   end do  ! total_radar

   if (print_detail_radar) write (unit=stdout,fmt='(/,A,I3,/)') &
       ' Processed TOTAL RADAR: ', total_radar
   close(iunit)
   call da_free_unit(iunit)
   if (trace_use) call da_trace_exit("da_read_obs_radar")
end subroutine da_read_obs_radar

